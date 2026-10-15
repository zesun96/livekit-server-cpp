#include "detail/crypto.h"
#include "livekit/server/access_token.h"
#include "livekit/server/error.h"
#include "livekit/server/livekit_api.h"

#include <google/protobuf/struct.pb.h>
#include <google/protobuf/util/json_util.h>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef _WIN32
#define LKS_RUBY_EXPORT __declspec(dllexport)
#else
#define LKS_RUBY_EXPORT __attribute__((visibility("default")))
#endif

namespace {
using namespace livekit::server;
using ObjectType = google::protobuf::Struct;
using Value = google::protobuf::Value;
// Borrowed UTF-8 JSON result, valid until the enclosing invoke returns. Callback requests are
// synchronous on the invoking thread. No Ruby objects, closures or pointers survive that call.
using Callback = const char* (*)(const char* request, std::size_t length);

Value Text(std::string_view text) {
	Value value;
	value.set_string_value(std::string(text));
	return value;
}

bool Has(const ObjectType& obj, const char* name) { return obj.fields().contains(name); }

const Value& Field(const ObjectType& obj, const char* name) {
	static const Value empty;
	const auto found = obj.fields().find(name);
	return found == obj.fields().end() ? empty : found->second;
}

void RequireKind(const Value& value, Value::KindCase kind) {
	if (value.kind_case() != kind) {
		throw Error(ErrorCode::invalid_argument, "invalid Ruby bridge field type");
	}
}

std::string String(const ObjectType& obj, const char* name) {
	const auto& value = Field(obj, name);
	if (value.kind_case() == Value::KIND_NOT_SET)
		return {};
	RequireKind(value, Value::kStringValue);
	return value.string_value();
}

bool Boolean(const ObjectType& obj, const char* name) {
	const auto& value = Field(obj, name);
	if (value.kind_case() == Value::KIND_NOT_SET)
		return false;
	RequireKind(value, Value::kBoolValue);
	return value.bool_value();
}

const ObjectType& Object(const ObjectType& obj, const char* name) {
	const auto& value = Field(obj, name);
	if (value.kind_case() != Value::KIND_NOT_SET)
		RequireKind(value, Value::kStructValue);
	return value.struct_value();
}

std::vector<std::string> StringList(const ObjectType& obj, const char* name) {
	std::vector<std::string> result;
	const auto& value = Field(obj, name);
	if (value.kind_case() == Value::KIND_NOT_SET)
		return result;
	RequireKind(value, Value::kListValue);
	for (const auto& item : value.list_value().values()) {
		RequireKind(item, Value::kStringValue);
		result.push_back(item.string_value());
	}
	return result;
}

std::map<std::string, std::string> StringMap(const ObjectType& obj) {
	std::map<std::string, std::string> result;
	for (const auto& [key, value] : obj.fields()) {
		RequireKind(value, Value::kStringValue);
		result.emplace(key, value.string_value());
	}
	return result;
}

std::int64_t Integer(const ObjectType& obj, const char* name) {
	const auto text = String(obj, name);
	try {
		std::size_t end{};
		const auto result = std::stoll(text, &end);
		if (end == text.size())
			return result;
	} catch (const std::exception&) {
	}
	throw Error(ErrorCode::invalid_argument, "invalid integer in Ruby bridge options");
}

ObjectType Parse(std::string_view json) {
	ObjectType obj;
	const auto status = google::protobuf::util::JsonStringToMessage(std::string(json), &obj);
	if (!status.ok())
		throw Error(ErrorCode::invalid_argument, "invalid Ruby bridge JSON");
	return obj;
}

std::string Encode(const ObjectType& obj) {
	std::string json;
	const auto status = google::protobuf::util::MessageToJsonString(obj, &json);
	if (!status.ok())
		throw Error(ErrorCode::protocol, "could not encode Ruby bridge result");
	return json;
}

Value NewObject(std::initializer_list<std::pair<const char*, std::string_view>> fields) {
	Value result;
	for (const auto& [key, value] : fields) {
		(*result.mutable_struct_value()->mutable_fields())[key] = Text(value);
	}
	return result;
}

std::string DecodeBody(const ObjectType& obj, const char* name) {
	const auto bytes = detail::Base64UrlDecode(String(obj, name));
	return {bytes.begin(), bytes.end()};
}

ObjectType Exchange(Callback callback, const ObjectType& request) {
	if (!callback)
		throw Error(ErrorCode::invalid_argument, "Ruby callback is required");
	const auto json = Encode(request);
	const char* response = callback(json.data(), json.size());
	if (!response)
		throw Error(ErrorCode::transport, "Ruby callback returned no response");
	auto result = Parse(response);
	if (Boolean(result, "error")) {
		throw Error(ErrorCode::transport, "Ruby callback failed");
	}
	return result;
}

class RubyTransport final : public HttpTransport {
public:
	explicit RubyTransport(Callback callback) : callback_(callback) {}
	HttpResponse Send(const HttpRequest& request) override {
		ObjectType data;
		auto& fields = *data.mutable_fields();
		fields["kind"] = Text("transport");
		fields["url"] = Text(request.url);
		fields["method"] = Text(request.method);
		fields["body"] = Text(detail::Base64UrlEncode(request.body));
		fields["timeout_ms"] = Text(std::to_string(request.timeout.count()));
		auto* headers = fields["headers"].mutable_list_value();
		for (const auto& [name, value] : request.headers) {
			auto* pair = headers->add_values()->mutable_list_value();
			*pair->add_values() = Text(name);
			*pair->add_values() = Text(value);
		}
		const auto response = Exchange(callback_, data);
		const auto status = Integer(response, "status_code");
		if (status < 100 || status > 599) {
			throw Error(ErrorCode::transport, "Ruby transport returned invalid HTTP status");
		}
		return HttpResponse{static_cast<int>(status), {}, DecodeBody(response, "body")};
	}

private:
	Callback callback_;
};

ApiOptions Options(const ObjectType& obj, Callback callback) {
	ApiOptions options;
	options.url = String(obj, "url");
	options.api_key = String(obj, "api_key");
	options.api_secret = String(obj, "api_secret");
	options.access_token = String(obj, "access_token");
	options.timeout = std::chrono::milliseconds(Integer(obj, "timeout_ms"));
	if (Boolean(obj, "use_transport"))
		options.transport = std::make_shared<RubyTransport>(callback);
	return options;
}

std::string Call(LiveKitApi& api, const std::string& operation, const std::string& request) {
#include "dispatch.inc"
	throw Error(ErrorCode::invalid_argument, "unknown Ruby server operation");
}

std::string Sign(const ObjectType& options) {
	AccessToken token(String(options, "api_key"), String(options, "api_secret"));
	token.SetIdentity(String(options, "identity"))
	    .SetName(String(options, "name"))
	    .SetMetadata(String(options, "metadata"))
	    .SetAttributes(StringMap(Object(options, "attributes")));
	const auto ttl = Integer(options, "valid_for");
	const auto now = Integer(options, "now");
	using Clock = std::chrono::system_clock;
	const auto max =
	    std::chrono::duration_cast<std::chrono::seconds>(Clock::duration::max()).count();
	if (now < 0 || ttl <= 0 || ttl > max || now > max - ttl) {
		throw Error(ErrorCode::invalid_argument,
		            "token timestamp or lifetime exceeds native clock range");
	}
	token.SetValidFor(std::chrono::seconds(ttl));
#include "grants.inc"
	return token.ToJwt(Clock::time_point(std::chrono::seconds(now)));
}

Value Receive(const ObjectType& options, Callback callback) {
	std::unique_ptr<WebhookReceiver> receiver;
	if (Boolean(options, "use_key_provider")) {
		receiver = std::make_unique<WebhookReceiver>([&](const std::string& key) {
			ObjectType request;
			(*request.mutable_fields())["kind"] = Text("key");
			(*request.mutable_fields())["key"] = Text(key);
			return String(Exchange(callback, request), "secret");
		});
	} else {
		receiver = std::make_unique<WebhookReceiver>(StringMap(Object(options, "secrets")));
	}
	const auto event =
	    receiver->Receive(DecodeBody(options, "body"), String(options, "authorization"));
	Value result =
	    NewObject({{"event", event.event}, {"id", event.id}, {"raw_body", event.raw_body}});
	auto& fields = *result.mutable_struct_value()->mutable_fields();
	fields["created_at"] = Text(std::to_string(event.created_at));
	if (event.room)
		fields["room"] = NewObject({{"sid", event.room->sid},
		                            {"name", event.room->name},
		                            {"metadata", event.room->metadata}});
	if (event.participant)
		fields["participant"] = NewObject({{"sid", event.participant->sid},
		                                   {"identity", event.participant->identity},
		                                   {"name", event.participant->name},
		                                   {"metadata", event.participant->metadata}});
	if (event.track)
		fields["track"] = NewObject({{"sid", event.track->sid}, {"name", event.track->name}});
	if (event.egress)
		fields["egress"] = NewObject({{"egress_id", event.egress->egress_id},
		                              {"room_id", event.egress->room_id},
		                              {"room_name", event.egress->room_name}});
	if (event.ingress)
		fields["ingress"] = NewObject({{"ingress_id", event.ingress->ingress_id},
		                               {"name", event.ingress->name},
		                               {"room_name", event.ingress->room_name}});
	return result;
}

Value Execute(const ObjectType& input, Callback callback) {
	const auto operation = String(input, "operation");
	const auto& options = Object(input, "options");
	if (operation == "token")
		return Text(Sign(options));
	if (operation == "webhook")
		return Receive(options, callback);
	LiveKitApi api(Options(options, callback));
	if (operation == "get_dispatch") {
		const auto result = api.AgentDispatch().GetDispatchModel(String(input, "dispatch_id"),
		                                                         String(input, "room"));
		if (result)
			return Text(result->Json());
		Value empty;
		empty.set_null_value(google::protobuf::NULL_VALUE);
		return empty;
	}
	return Text(Call(api, operation, String(input, "request")));
}

ObjectType Failure(const Error& error) {
	ObjectType response;
	auto& fields = *response.mutable_fields();
	fields["ok"].set_bool_value(false);
	fields["code"].set_number_value(static_cast<int>(error.code()));
	fields["http_status"].set_number_value(error.http_status());
	fields["twirp_code"] = Text(error.twirp_code());
	fields["message"] = Text(error.what());
	return response;
}

void* Copy(const ObjectType& result) {
	const auto json = Encode(result);
	auto* output = std::malloc(json.size() + 1);
	if (output)
		std::memcpy(output, json.c_str(), json.size() + 1);
	return output;
}
} // namespace

// Private, versioned bridge ABI. Allocations must be freed by this library, never by Ruby's CRT.
extern "C" {
LKS_RUBY_EXPORT int lks_ruby_abi_version() noexcept { return 1; }

LKS_RUBY_EXPORT void lks_ruby_free(void* output) noexcept { std::free(output); }

LKS_RUBY_EXPORT void* lks_ruby_invoke(const char* input, std::size_t length,
                                      Callback callback) noexcept {
	try {
		ObjectType response;
		try {
			if (!input || length > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
				throw Error(ErrorCode::invalid_argument, "invalid Ruby bridge input buffer");
			}
			(*response.mutable_fields())["value"] = Execute(Parse({input, length}), callback);
			(*response.mutable_fields())["ok"].set_bool_value(true);
		} catch (const Error& error) {
			response = Failure(error);
		} catch (const std::exception& error) {
			response = Failure(Error(ErrorCode::unknown, error.what()));
		} catch (...) {
			response = Failure(Error(ErrorCode::unknown, "unknown native Ruby bridge failure"));
		}
		return Copy(response);
	} catch (...) {
		return nullptr;
	}
}
}
