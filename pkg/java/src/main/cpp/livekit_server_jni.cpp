#include <jni.h>

#include "livekit/server/access_token.h"
#include "livekit/server/error.h"
#include "livekit/server/livekit_api.h"

#include <google/protobuf/struct.pb.h>
#include <google/protobuf/util/json_util.h>

#include <chrono>
#include <initializer_list>
#include <limits>
#include <map>
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

std::map<std::string, std::string> Strings(const Struct& obj) {
	std::map<std::string, std::string> result;
	for (const auto& [key, value] : obj.fields()) {
		if (value.kind_case() != Value::kStringValue) {
			throw Error(ErrorCode::invalid_argument, "expected string map value");
		}
		result.emplace(key, value.string_value());
	}
	return result;
}

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

ApiOptions Options(JNIEnv* env, jbyteArray json, jobject transport) {
	const auto obj = Parse(env, json);
	ApiOptions options;
	options.url = String(obj, "url");
	options.api_key = String(obj, "apiKey");
	options.api_secret = String(obj, "apiSecret");
	options.access_token = String(obj, "accessToken");
	options.timeout = std::chrono::milliseconds(std::stoll(String(obj, "timeoutMs")));
	if (transport) {
		options.transport = std::make_shared<JavaTransport>(env, transport);
	}
	return options;
}

std::string Call(LiveKitApi& api, const std::string& operation, const std::string& request) {
#include "dispatch.inc"
	throw Error(ErrorCode::invalid_argument, "unknown server operation");
}

AccessToken Token(const Struct& obj) {
	AccessToken token(String(obj, "apiKey"), String(obj, "apiSecret"));
	token.SetIdentity(String(obj, "identity"))
	    .SetName(String(obj, "name"))
	    .SetMetadata(String(obj, "metadata"))
	    .SetAttributes(Strings(Field(obj, "attributes").struct_value()));
	if (obj.fields().contains("validFor")) {
		token.SetValidFor(std::chrono::seconds(std::stoll(String(obj, "validFor"))));
	}
	if (obj.fields().contains("video")) {
		const auto& v = Field(obj, "video").struct_value();
		VideoGrant grant;
		grant.room_create = Bool(v, "roomCreate");
		grant.room_list = Bool(v, "roomList");
		grant.room_record = Bool(v, "roomRecord");
		grant.room_admin = Bool(v, "roomAdmin");
		grant.room_join = Bool(v, "roomJoin");
		grant.room = String(v, "room");
		grant.destination_room = String(v, "destinationRoom");
		grant.ingress_admin = Bool(v, "ingressAdmin");
		if (v.fields().contains("canPublish"))
			grant.can_publish = Bool(v, "canPublish");
		if (v.fields().contains("canSubscribe"))
			grant.can_subscribe = Bool(v, "canSubscribe");
		if (v.fields().contains("canPublishData"))
			grant.can_publish_data = Bool(v, "canPublishData");
		if (v.fields().contains("canUpdateOwnMetadata")) {
			grant.can_update_own_metadata = Bool(v, "canUpdateOwnMetadata");
		}
		for (const auto& value : Field(v, "canPublishSources").list_value().values()) {
			grant.can_publish_sources.push_back(value.string_value());
		}
		grant.hidden = Bool(v, "hidden");
		grant.recorder = Bool(v, "recorder");
		grant.agent = Bool(v, "agent");
		token.SetVideoGrant(std::move(grant));
	}
	if (obj.fields().contains("sip")) {
		const auto& v = Field(obj, "sip").struct_value();
		token.SetSipGrant({Bool(v, "admin"), Bool(v, "call")});
	}
	if (obj.fields().contains("agent")) {
		const auto& v = Field(obj, "agent").struct_value();
		token.SetAgentGrant({Bool(v, "admin"), Bool(v, "simulationAdmin")});
	}
	return token;
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
		LiveKitApi api(Options(env, options, transport));
		return Bytes(env, Call(api, op, Bytes(env, request)));
	});
}

JNIEXPORT jbyteArray JNICALL Java_io_livekit_server_Native_getDispatch(
    JNIEnv* env, jclass, jbyteArray options, jbyteArray id, jbyteArray room, jobject transport) {
	return Guard(env, [&]() -> jbyteArray {
		LiveKitApi api(Options(env, options, transport));
		const auto result = api.AgentDispatch().GetDispatchModel(Bytes(env, id), Bytes(env, room));
		return result ? Bytes(env, result->Json()) : nullptr;
	});
}

JNIEXPORT jbyteArray JNICALL Java_io_livekit_server_Native_token(JNIEnv* env, jclass,
                                                                 jbyteArray options, jlong now) {
	return Guard(env, [&]() -> jbyteArray {
		// system_clock may have finer resolution than seconds. Bound both timestamp and expiry
		// before conversion to avoid overflowing the native clock representation.
		using Clock = std::chrono::system_clock;
		const auto max =
		    std::chrono::duration_cast<std::chrono::seconds>(Clock::duration::max()).count();
		const auto obj = Parse(env, options);
		const auto ttl =
		    obj.fields().contains("validFor") ? std::stoll(String(obj, "validFor")) : 21600;
		if (now < 0 || ttl <= 0 || ttl > max || now > max - ttl) {
			throw Error(ErrorCode::invalid_argument,
			            "token timestamp or lifetime exceeds native clock range");
		}
		return Bytes(env, Token(obj).ToJwt(Clock::time_point(std::chrono::seconds(now))));
	});
}

JNIEXPORT jobject JNICALL Java_io_livekit_server_Native_receive(JNIEnv* env, jclass,
                                                                jbyteArray secrets, jbyteArray body,
                                                                jbyteArray authorization,
                                                                jobject key_provider) {
	return Guard(env, [&]() -> jobject {
		if (env->EnsureLocalCapacity(64) != JNI_OK)
			throw PendingException{};
		std::unique_ptr<WebhookReceiver> receiver;
		if (key_provider) {
			receiver = std::make_unique<WebhookReceiver>([&](const std::string& key) {
				auto cls = env->FindClass("java/util/function/Function");
				Check(env);
				auto apply =
				    env->GetMethodID(cls, "apply", "(Ljava/lang/Object;)Ljava/lang/Object;");
				Check(env);
				auto issuer = Text(env, key);
				auto secret =
				    static_cast<jstring>(env->CallObjectMethod(key_provider, apply, issuer));
				Check(env);
				return secret ? Utf8(env, secret) : std::string{};
			});
		} else {
			receiver = std::make_unique<WebhookReceiver>(Strings(Parse(env, secrets)));
		}
		const auto event = receiver->Receive(Bytes(env, body), Bytes(env, authorization));
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
