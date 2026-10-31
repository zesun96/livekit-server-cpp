#include "detail/crypto.h"
#include "livekit/server/c_api.h"
#include "livekit/server/error.h"
#include "livekit/server/http_transport.h"

#include <google/protobuf/struct.pb.h>
#include <google/protobuf/util/json_util.h>
#include <gtest/gtest.h>

#include <cstring>
#include <future>
#include <limits>
#include <memory>
#include <string>
#include <thread>

namespace {
using Object = google::protobuf::Struct;
Object Invoke(const char* input, std::size_t length, lks_callback callback = nullptr,
              void* user = nullptr) {
	std::size_t output_length = 999;
	std::unique_ptr<void, decltype(&lks_free)> result(
	    lks_invoke(input, length, callback, user, &output_length), lks_free);
	EXPECT_NE(result, nullptr);
	Object output;
	if (result) {
		EXPECT_EQ(std::strlen(static_cast<const char*>(result.get())), output_length);
		EXPECT_TRUE(google::protobuf::util::JsonStringToMessage(
		                std::string(static_cast<const char*>(result.get()), output_length), &output)
		                .ok());
	}
	return output;
}

constexpr char request[] =
    R"({"operation":"Room.ListRooms","request":"{}","options":{"url":"http://example.invalid","access_token":"test-token","timeout_ms":"1000","use_transport":true}})";

struct Context {
	std::string response = R"({"status_code":"200","body":""})";
	int calls = 0;
	bool fail = false;
	bool oversized = false;
	std::thread::id thread = std::this_thread::get_id();
};

int32_t Callback(void* user, const char* input, std::size_t length, const char** response,
                 std::size_t* size) {
	auto& ctx = *static_cast<Context*>(user);
	EXPECT_EQ(ctx.thread, std::this_thread::get_id());
	EXPECT_NE(std::string_view(input, length).find("transport"), std::string_view::npos);
	++ctx.calls;
	// Nested calls have independent results, errors and state.
	auto nested = Invoke("{", 1);
	EXPECT_FALSE(nested.fields().at("ok").bool_value());
	*response = ctx.response.data();
	*size = ctx.oversized ? std::numeric_limits<std::size_t>::max() : ctx.response.size();
	return ctx.fail ? 1 : 0;
}

TEST(CApi, VersionErrorsAndNullFree) {
	EXPECT_EQ(lks_abi_version(), 1);
	lks_free(nullptr);
	for (const auto& result : {Invoke(nullptr, 0), Invoke("{", 1), Invoke("{}", 2),
	                           Invoke("x", std::numeric_limits<std::size_t>::max())}) {
		EXPECT_FALSE(result.fields().at("ok").bool_value());
		EXPECT_EQ(result.fields().at("code").number_value(), LKS_INVALID_ARGUMENT);
		EXPECT_TRUE(result.fields().contains("message"));
		EXPECT_TRUE(result.fields().contains("http_status"));
		EXPECT_TRUE(result.fields().contains("twirp_code"));
	}
}

TEST(CApi, SynchronousContextCallbackAndLengthDelimitedResponse) {
	Context ctx;
	// An extra non-JSON suffix proves the callback response is read using its byte length.
	ctx.response += 'x';
	auto callback = +[](void* user, const char* input, std::size_t length, const char** response,
	                    std::size_t* size) -> int32_t {
		auto status = Callback(user, input, length, response, size);
		--*size;
		return status;
	};
	const auto result = Invoke(request, sizeof(request) - 1, callback, &ctx);
	ASSERT_TRUE(result.fields().at("ok").bool_value()) << result.DebugString();
	EXPECT_EQ(ctx.calls, 1);
	EXPECT_EQ(result.fields().at("value").string_value(), "{}");
}

TEST(CApi, CallbackFailuresAndHttpError) {
	for (int mode = 0; mode < 4; ++mode) {
		Context ctx;
		ctx.fail = mode == 0;
		ctx.oversized = mode == 1;
		if (mode == 2)
			ctx.response = R"({"status_code":"999","body":""})";
		if (mode == 3)
			ctx.response =
			    R"({"status_code":"403","body":"eyJjb2RlIjoicGVybWlzc2lvbl9kZW5pZWQiLCJtc2ciOiJkZW5pZWQifQ"})";
		const auto result = Invoke(request, sizeof(request) - 1, Callback, &ctx);
		EXPECT_FALSE(result.fields().at("ok").bool_value());
		EXPECT_EQ(result.fields().at("code").number_value(), mode == 3 ? LKS_HTTP : LKS_TRANSPORT);
		if (mode == 3) {
			EXPECT_EQ(result.fields().at("http_status").number_value(), 403);
			EXPECT_EQ(result.fields().at("twirp_code").string_value(), "permission_denied");
		}
	}
	const auto result = Invoke(request, sizeof(request) - 1);
	EXPECT_FALSE(result.fields().at("ok").bool_value());
}

TEST(CApi, ConcurrentInvokesHaveIndependentState) {
	auto run = [] {
		Context ctx;
		auto result = Invoke(request, sizeof(request) - 1, Callback, &ctx);
		ASSERT_TRUE(result.fields().at("ok").bool_value()) << result.DebugString();
		EXPECT_EQ(ctx.calls, 1);
	};
	auto first = std::async(std::launch::async, run);
	auto second = std::async(std::launch::async, run);
	first.get();
	second.get();
}

TEST(CryptoBackend, IndependentKnownAnswerVectors) {
	using namespace livekit::server::detail;
	EXPECT_EQ(Base64Encode(Sha256("abc")), "ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa0=");
	// RFC 4231 test case 1.
	EXPECT_EQ(Base64Encode(HmacSha256(std::string(20, '\x0b'), "Hi There")),
	          "sDRMYdjbOFNcqK/OrwvxK4gdwgDJgz2nJuk3bC4yz/c=");
}

#ifdef LIVEKIT_SERVER_TEST_CURL
TEST(CurlBackend, RejectsInvalidRequestsBeforeOpeningConnections) {
	using namespace livekit::server;
	const auto transport = CreateDefaultHttpTransport();
	auto expect_invalid = [&](const HttpRequest& request) {
		try {
			(void)transport->Send(request);
			FAIL() << "expected request rejection";
		} catch (const Error& error) {
			EXPECT_EQ(error.code(), ErrorCode::invalid_argument);
		}
	};
	HttpRequest request;
	request.url = "file:///etc/passwd";
	expect_invalid(request);
	request.url = "http://example.invalid";
	request.method = "POST\r\nInjected: true";
	expect_invalid(request);
	request.method = "POST";
	request.timeout = std::chrono::milliseconds(0);
	expect_invalid(request);
	request.timeout = std::chrono::milliseconds(100);
	request.headers = {{"Authorization", "Bearer test\r\nInjected: true"}};
	expect_invalid(request);
	request.headers = {{"Invalid Name", "value"}};
	expect_invalid(request);
	request.headers.clear();
	request.url.append(1, '\0');
	expect_invalid(request);
}
#endif
} // namespace
