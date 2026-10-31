#include <jni.h>

#include "detail/crypto.h"
#include "livekit/server/c_api.h"
#include "livekit/server/error.h"
#include "livekit/server/http_transport.h"
#include "livekit/server/webhook_receiver.h"

#include <google/protobuf/struct.pb.h>
#include <google/protobuf/util/json_util.h>

#include <chrono>
#include <deque>
#include <exception>
#include <initializer_list>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace livekit::server;
using Struct = google::protobuf::Struct;
using Value = google::protobuf::Value;

struct PendingException {};

void Check(JNIEnv* env) {
	if (env->ExceptionCheck()) {
		throw PendingException{};
	}
}

std::string Bytes(JNIEnv* env, jbyteArray bytes) {
	if (!bytes) {
		throw Error(ErrorCode::invalid_argument, "byte array must not be null");
	}
	const auto size = env->GetArrayLength(bytes);
	std::string result(static_cast<std::size_t>(size), '\0');
	if (size) {
		env->GetByteArrayRegion(bytes, 0, size, reinterpret_cast<jbyte*>(result.data()));
		Check(env);
	}
	return result;
}

jbyteArray Bytes(JNIEnv* env, std::string_view text) {
	if (text.size() > static_cast<std::size_t>(std::numeric_limits<jsize>::max())) {
		throw Error(ErrorCode::invalid_argument, "native result exceeds Java array size");
	}
	auto bytes = env->NewByteArray(static_cast<jsize>(text.size()));
	Check(env);
	if (!text.empty()) {
		env->SetByteArrayRegion(bytes, 0, static_cast<jsize>(text.size()),
		                        reinterpret_cast<const jbyte*>(text.data()));
		Check(env);
	}
	return bytes;
}

// JNI modified UTF-8 cannot represent arbitrary UTF-8 JSON. Decode using Java's UTF-8 codec.
jstring Text(JNIEnv* env, std::string_view text) {
	auto bytes = Bytes(env, text);
	auto cls = env->FindClass("io/livekit/server/Native");
	Check(env);
	auto method = env->GetStaticMethodID(cls, "text", "([B)Ljava/lang/String;");
	Check(env);
	auto result = static_cast<jstring>(env->CallStaticObjectMethod(cls, method, bytes));
	Check(env);
	env->DeleteLocalRef(bytes);
	env->DeleteLocalRef(cls);
	return result;
}

std::string Utf8(JNIEnv* env, jstring text) {
	auto cls = env->FindClass("io/livekit/server/Native");
	Check(env);
	auto method = env->GetStaticMethodID(cls, "bytes", "(Ljava/lang/String;)[B");
	Check(env);
	auto bytes = static_cast<jbyteArray>(env->CallStaticObjectMethod(cls, method, text));
	Check(env);
	auto result = Bytes(env, bytes);
	env->DeleteLocalRef(bytes);
	env->DeleteLocalRef(cls);
	return result;
}

jobject Record(JNIEnv* env, const char* name, std::initializer_list<std::string_view> fields) {
	auto cls = env->FindClass(name);
	Check(env);
	std::string signature = "(";
	std::vector<jvalue> args;
	for (const auto& field : fields) {
		signature += "Ljava/lang/String;";
		jvalue value{};
		value.l = Text(env, field);
		args.push_back(value);
	}
	signature += ")V";
	auto ctor = env->GetMethodID(cls, "<init>", signature.c_str());
	Check(env);
	auto result = env->NewObjectA(cls, ctor, args.data());
	Check(env);
	for (const auto& arg : args)
		env->DeleteLocalRef(arg.l);
	env->DeleteLocalRef(cls);
	return result;
}

void Raise(JNIEnv* env, const Error& error) {
	auto cls = env->FindClass("io/livekit/server/LiveKitException");
	Check(env);
	auto ctor = env->GetMethodID(cls, "<init>", "(Ljava/lang/String;IILjava/lang/String;)V");
	Check(env);
	auto message = Text(env, error.what());
	auto twirp = Text(env, error.twirp_code());
	auto exception = static_cast<jthrowable>(env->NewObject(
	    cls, ctor, message, static_cast<jint>(error.code()), error.http_status(), twirp));
	Check(env);
	env->Throw(exception);
}

template <typename F> auto Guard(JNIEnv* env, F&& fn) -> decltype(fn()) {
	try {
		return fn();
	} catch (const PendingException&) {
		return nullptr;
	} catch (...) {
		try {
			try {
				throw;
			} catch (const Error& error) {
				Raise(env, error);
			} catch (const std::exception& error) {
				Raise(env, Error(ErrorCode::unknown, error.what()));
			} catch (...) {
				Raise(env, Error(ErrorCode::unknown, "unknown native failure"));
			}
		} catch (...) {
			// Preserve any pending JVM exception (including allocation failures).
		}
		return nullptr;
	}
}

Struct Parse(JNIEnv* env, jbyteArray json) {
	Struct result;
	const auto status = google::protobuf::util::JsonStringToMessage(Bytes(env, json), &result);
	if (!status.ok()) {
		throw Error(ErrorCode::invalid_argument, "invalid binding options JSON");
	}
	return result;
}

const Value& Field(const Struct& obj, const char* key) {
	static const Value empty;
	const auto found = obj.fields().find(key);
	return found == obj.fields().end() ? empty : found->second;
}

std::string String(const Struct& obj, const char* key) { return Field(obj, key).string_value(); }
bool Bool(const Struct& obj, const char* key) { return Field(obj, key).bool_value(); }

// Borrowed JVM references live only for the synchronous native call. No thread is attached,
// no global references are retained, and Java transport exceptions pass through unchanged.
class JavaTransport final : public HttpTransport {
public:
	JavaTransport(JNIEnv* env, jobject transport) : env_(env), transport_(transport) {}
	HttpResponse Send(const HttpRequest& request) override {
		if (env_->PushLocalFrame(32) != JNI_OK) {
			throw PendingException{};
		}
		struct Frame {
			JNIEnv* env;
			~Frame() { env->PopLocalFrame(nullptr); }
		} frame{env_};
		auto cls = env_->FindClass("io/livekit/server/HttpTransport");
		Check(env_);
		auto send = env_->GetMethodID(cls, "send",
		                              "(Ljava/lang/String;Ljava/lang/String;[Ljava/lang/"
		                              "String;[BJ)Lio/livekit/server/HttpResponse;");
		Check(env_);
		auto string_cls = env_->FindClass("java/lang/String");
		Check(env_);
		if (request.headers.size() >
		    static_cast<std::size_t>(std::numeric_limits<jsize>::max() / 2)) {
			throw Error(ErrorCode::invalid_argument, "too many request headers");
		}
		auto headers = env_->NewObjectArray(static_cast<jsize>(request.headers.size() * 2),
		                                    string_cls, nullptr);
		Check(env_);
		jsize index = 0;
		for (const auto& [key, value] : request.headers) {
			for (const auto& text : {key, value}) {
				auto item = Text(env_, text);
				env_->SetObjectArrayElement(headers, index++, item);
				Check(env_);
				env_->DeleteLocalRef(item);
			}
		}
		auto url = Text(env_, request.url);
		auto method = Text(env_, request.method);
		auto body = Bytes(env_, request.body);
		auto response = env_->CallObjectMethod(transport_, send, url, method, headers, body,
		                                       static_cast<jlong>(request.timeout.count()));
		Check(env_);
		if (!response) {
			throw Error(ErrorCode::transport, "Java transport returned null");
		}
		auto response_cls = env_->FindClass("io/livekit/server/HttpResponse");
		Check(env_);
		auto status = env_->GetMethodID(response_cls, "statusCode", "()I");
		Check(env_);
		auto get_body = env_->GetMethodID(response_cls, "body", "()[B");
		Check(env_);
		HttpResponse result;
		result.status_code = env_->CallIntMethod(response, status);
		Check(env_);
		auto response_body = static_cast<jbyteArray>(env_->CallObjectMethod(response, get_body));
		Check(env_);
		result.body = Bytes(env_, response_body);
		return result;
	}

private:
	JNIEnv* env_;
	jobject transport_;
};

Value TextValue(std::string_view text) {
	Value result;
	result.set_string_value(std::string(text));
	return result;
}

std::string Encode(const Struct& value) {
	std::string result;
	if (!google::protobuf::util::MessageToJsonString(value, &result).ok())
		throw Error(ErrorCode::protocol, "could not encode binding JSON");
	return result;
}

Struct ParseJson(std::string_view json) {
	Struct result;
	if (!google::protobuf::util::JsonStringToMessage(std::string(json), &result).ok())
		throw Error(ErrorCode::protocol, "invalid C ABI response");
	return result;
}

// Translate Java option names only; attribute keys and protocol JSON remain verbatim.
Struct Options(const Struct& source, bool grants = false) {
	Struct result;
	for (const auto& [key, value] : source.fields()) {
		std::string name;
		for (unsigned char ch : key) {
			if (ch >= 'A' && ch <= 'Z') {
				name += '_';
				name += static_cast<char>(ch + 'a' - 'A');
			} else
				name += static_cast<char>(ch);
		}
		auto& target = (*result.mutable_fields())[name];
		if (grants && (key == "video" || key == "sip" || key == "agent"))
			*target.mutable_struct_value() = Options(value.struct_value());
		else
			target = value;
	}
	return result;
}

struct CallbackContext {
	JNIEnv* env;
	jobject transport;
	jobject key_provider;
	std::deque<std::string> responses;
	std::exception_ptr failure;
};

int32_t Exchange(void* user, const char* json, std::size_t length, const char** output,
                 std::size_t* size) noexcept {
	auto& context = *static_cast<CallbackContext*>(user);
	try {
		const auto input = ParseJson({json, length});
		Struct response;
		auto& fields = *response.mutable_fields();
		if (String(input, "kind") == "transport") {
			HttpRequest request;
			request.url = String(input, "url");
			request.method = String(input, "method");
			const auto body = detail::Base64UrlDecode(String(input, "body"));
			request.body.assign(body.begin(), body.end());
			request.timeout = std::chrono::milliseconds(std::stoll(String(input, "timeout_ms")));
			for (const auto& pair : Field(input, "headers").list_value().values())
				request.headers.emplace_back(pair.list_value().values(0).string_value(),
				                             pair.list_value().values(1).string_value());
			const auto result = JavaTransport(context.env, context.transport).Send(request);
			fields["status_code"] = TextValue(std::to_string(result.status_code));
			fields["body"] = TextValue(detail::Base64UrlEncode(result.body));
		} else {
			auto* env = context.env;
			if (env->PushLocalFrame(16) != JNI_OK)
				throw PendingException{};
			struct Frame {
				JNIEnv* env;
				~Frame() { env->PopLocalFrame(nullptr); }
			} frame{env};
			auto cls = env->FindClass("java/util/function/Function");
			Check(env);
			auto apply = env->GetMethodID(cls, "apply", "(Ljava/lang/Object;)Ljava/lang/Object;");
			Check(env);
			auto issuer = Text(env, String(input, "key"));
			auto secret =
			    static_cast<jstring>(env->CallObjectMethod(context.key_provider, apply, issuer));
			Check(env);
			fields["secret"] = TextValue(secret ? Utf8(env, secret) : std::string{});
		}
		context.responses.push_back(Encode(response));
		*output = context.responses.back().data();
		*size = context.responses.back().size();
		return 0;
	} catch (...) {
		context.failure = std::current_exception();
		return 1;
	}
}

Value Invoke(const Struct& input, CallbackContext& context) {
	const auto json = Encode(input);
	std::size_t length = 0;
	std::unique_ptr<void, decltype(&lks_free)> output(
	    lks_invoke(json.data(), json.size(), Exchange, &context, &length), lks_free);
	if (context.failure)
		std::rethrow_exception(context.failure);
	if (!output)
		throw std::bad_alloc();
	const auto response = ParseJson({static_cast<const char*>(output.get()), length});
	if (!Bool(response, "ok")) {
		throw Error(
		    static_cast<ErrorCode>(static_cast<int>(Field(response, "code").number_value())),
		    String(response, "message"),
		    static_cast<int>(Field(response, "http_status").number_value()),
		    String(response, "twirp_code"));
	}
	return Field(response, "value");
}

Struct Input(const char* operation, Struct options) {
	Struct input;
	(*input.mutable_fields())["operation"] = TextValue(operation);
	*(*input.mutable_fields())["options"].mutable_struct_value() = std::move(options);
	return input;
}

Value Call(JNIEnv* env, jbyteArray options, jobject transport, const char* operation,
           std::initializer_list<std::pair<const char*, std::string>> fields) {
	auto opts = Options(Parse(env, options));
	(*opts.mutable_fields())["use_transport"].set_bool_value(transport != nullptr);
	auto input = Input(operation, std::move(opts));
	for (const auto& [name, value] : fields)
		(*input.mutable_fields())[name] = TextValue(value);
	CallbackContext context{env, transport, nullptr, {}, {}};
	return Invoke(input, context);
}
} // namespace

extern "C" {
JNIEXPORT jbyteArray JNICALL Java_io_livekit_server_Native_call(JNIEnv* env, jclass,
                                                                jbyteArray options,
                                                                jstring operation,
                                                                jbyteArray request,
                                                                jobject transport) {
	return Guard(env, [&]() -> jbyteArray {
		if (!operation)
			throw Error(ErrorCode::invalid_argument, "operation must not be null");
		const char* chars = env->GetStringUTFChars(operation, nullptr);
		Check(env);
		std::string op;
		try {
			op = chars;
		} catch (...) {
			env->ReleaseStringUTFChars(operation, chars);
			throw;
		}
		env->ReleaseStringUTFChars(operation, chars);
		return Bytes(env,
		             Call(env, options, transport, op.c_str(), {{"request", Bytes(env, request)}})
		                 .string_value());
	});
}

JNIEXPORT jbyteArray JNICALL Java_io_livekit_server_Native_getDispatch(
    JNIEnv* env, jclass, jbyteArray options, jbyteArray id, jbyteArray room, jobject transport) {
	return Guard(env, [&]() -> jbyteArray {
		const auto result = Call(env, options, transport, "get_dispatch",
		                         {{"dispatch_id", Bytes(env, id)}, {"room", Bytes(env, room)}});
		return result.kind_case() == Value::kNullValue ? nullptr
		                                               : Bytes(env, result.string_value());
	});
}

JNIEXPORT jbyteArray JNICALL Java_io_livekit_server_Native_token(JNIEnv* env, jclass,
                                                                 jbyteArray options, jlong now) {
	return Guard(env, [&]() -> jbyteArray {
		auto opts = Options(Parse(env, options), true);
		if (!opts.fields().contains("valid_for"))
			(*opts.mutable_fields())["valid_for"] = TextValue("21600");
		(*opts.mutable_fields())["now"] = TextValue(std::to_string(now));
		CallbackContext context{env, nullptr, nullptr, {}, {}};
		return Bytes(env, Invoke(Input("token", std::move(opts)), context).string_value());
	});
}

JNIEXPORT jobject JNICALL Java_io_livekit_server_Native_receive(JNIEnv* env, jclass,
                                                                jbyteArray secrets, jbyteArray body,
                                                                jbyteArray authorization,
                                                                jobject key_provider) {
	return Guard(env, [&]() -> jobject {
		if (env->EnsureLocalCapacity(64) != JNI_OK)
			throw PendingException{};
		Struct opts;
		auto& fields = *opts.mutable_fields();
		fields["body"] = TextValue(detail::Base64UrlEncode(Bytes(env, body)));
		fields["authorization"] = TextValue(Bytes(env, authorization));
		fields["use_key_provider"].set_bool_value(key_provider != nullptr);
		if (!key_provider)
			*fields["secrets"].mutable_struct_value() = Parse(env, secrets);
		CallbackContext context{env, nullptr, key_provider, {}, {}};
		const auto value = Invoke(Input("webhook", std::move(opts)), context);
		const auto& obj = value.struct_value();
		WebhookEvent event;
		event.event = String(obj, "event");
		event.id = String(obj, "id");
		event.created_at = std::stoll(String(obj, "created_at"));
		event.raw_body = String(obj, "raw_body");
		if (obj.fields().contains("room")) {
			const auto& v = Field(obj, "room").struct_value();
			event.room = WebhookRoom{String(v, "sid"), String(v, "name"), String(v, "metadata")};
		}
		if (obj.fields().contains("participant")) {
			const auto& v = Field(obj, "participant").struct_value();
			event.participant = WebhookParticipant{String(v, "sid"), String(v, "identity"),
			                                       String(v, "name"), String(v, "metadata")};
		}
		if (obj.fields().contains("track")) {
			const auto& v = Field(obj, "track").struct_value();
			event.track = WebhookTrack{String(v, "sid"), String(v, "name")};
		}
		if (obj.fields().contains("egress")) {
			const auto& v = Field(obj, "egress").struct_value();
			event.egress =
			    WebhookEgress{String(v, "egress_id"), String(v, "room_id"), String(v, "room_name")};
		}
		if (obj.fields().contains("ingress")) {
			const auto& v = Field(obj, "ingress").struct_value();
			event.ingress =
			    WebhookIngress{String(v, "ingress_id"), String(v, "name"), String(v, "room_name")};
		}
		auto cls = env->FindClass("io/livekit/server/WebhookEvent");
		Check(env);
		auto ctor = env->GetMethodID(
		    cls, "<init>",
		    "(Ljava/lang/String;Ljava/lang/String;JLio/livekit/server/WebhookEvent$Room;"
		    "Lio/livekit/server/WebhookEvent$Participant;Lio/livekit/server/WebhookEvent$Track;"
		    "Lio/livekit/server/WebhookEvent$Egress;Lio/livekit/server/WebhookEvent$Ingress;"
		    "Ljava/lang/String;)V");
		Check(env);
		auto name = Text(env, event.event);
		auto id = Text(env, event.id);
		auto raw = Text(env, event.raw_body);
		auto room = event.room ? Record(env, "io/livekit/server/WebhookEvent$Room",
		                                {event.room->sid, event.room->name, event.room->metadata})
		                       : nullptr;
		auto participant = event.participant
		                       ? Record(env, "io/livekit/server/WebhookEvent$Participant",
		                                {event.participant->sid, event.participant->identity,
		                                 event.participant->name, event.participant->metadata})
		                       : nullptr;
		auto track = event.track ? Record(env, "io/livekit/server/WebhookEvent$Track",
		                                  {event.track->sid, event.track->name})
		                         : nullptr;
		auto egress =
		    event.egress
		        ? Record(env, "io/livekit/server/WebhookEvent$Egress",
		                 {event.egress->egress_id, event.egress->room_id, event.egress->room_name})
		        : nullptr;
		auto ingress =
		    event.ingress
		        ? Record(env, "io/livekit/server/WebhookEvent$Ingress",
		                 {event.ingress->ingress_id, event.ingress->name, event.ingress->room_name})
		        : nullptr;
		auto result = env->NewObject(cls, ctor, name, id, static_cast<jlong>(event.created_at),
		                             room, participant, track, egress, ingress, raw);
		Check(env);
		return result;
	});
}
} // extern "C"
