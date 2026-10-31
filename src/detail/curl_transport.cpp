#include "livekit/server/error.h"
#include "livekit/server/http_transport.h"

#include <curl/curl.h>

#include <algorithm>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <string_view>

namespace livekit::server {
namespace {
struct CurlRuntime {
	CurlRuntime() {
		if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
			throw Error(ErrorCode::transport, "could not initialize libcurl");
	}
	~CurlRuntime() { curl_global_cleanup(); }
};

void Check(CURLcode code) {
	if (code != CURLE_OK)
		throw Error(ErrorCode::transport, curl_easy_strerror(code));
}

struct ResponseState {
	HttpResponse response;
	std::exception_ptr failure;
};

std::size_t Body(char* data, std::size_t size, std::size_t count, void* user) noexcept {
	auto& state = *static_cast<ResponseState*>(user);
	try {
		if (size && count > std::numeric_limits<std::size_t>::max() / size)
			return 0;
		state.response.body.append(data, size * count);
		return size * count;
	} catch (...) {
		state.failure = std::current_exception();
		return 0;
	}
}

std::size_t Header(char* data, std::size_t size, std::size_t count, void* user) noexcept {
	auto& state = *static_cast<ResponseState*>(user);
	try {
		if (size && count > std::numeric_limits<std::size_t>::max() / size)
			return 0;
		const auto length = size * count;
		std::string_view line(data, length);
		if (line.starts_with("HTTP/"))
			state.response.headers.clear();
		const auto colon = line.find(':');
		if (colon != std::string_view::npos) {
			auto value = line.substr(colon + 1);
			const auto start = value.find_first_not_of(" \t");
			value = start == std::string_view::npos ? std::string_view{} : value.substr(start);
			const auto end = value.find_last_not_of(" \t\r\n");
			value = end == std::string_view::npos ? std::string_view{} : value.substr(0, end + 1);
			state.response.headers.emplace_back(line.substr(0, colon), value);
		}
		return length;
	} catch (...) {
		state.failure = std::current_exception();
		return 0;
	}
}

bool Token(std::string_view value) {
	if (value.empty())
		return false;
	for (unsigned char ch : value) {
		if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
		      std::string_view("!#$%&'*+-.^_`|~").find(ch) != std::string_view::npos))
			return false;
	}
	return true;
}

class CurlTransport final : public HttpTransport {
public:
	CurlTransport() {
		static CurlRuntime runtime;
		handle_.reset(curl_easy_init());
		if (!handle_)
			throw Error(ErrorCode::transport, "could not create libcurl handle");
	}

	HttpResponse Send(const HttpRequest& request) override {
		if (request.url.find('\0') != std::string::npos ||
		    !(request.url.starts_with("http://") || request.url.starts_with("https://")) ||
		    !Token(request.method) || request.timeout.count() <= 0 ||
		    request.body.size() >
		        static_cast<std::size_t>(std::numeric_limits<curl_off_t>::max())) {
			throw Error(ErrorCode::invalid_argument,
			            "invalid HTTP request URL, method, timeout or body size");
		}
		std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headers(nullptr,
		                                                                    curl_slist_free_all);
		for (const auto& [name, value] : request.headers) {
			if (!Token(name) || value.find_first_of("\r\n") != std::string::npos ||
			    value.find('\0') != std::string::npos)
				throw Error(ErrorCode::invalid_argument, "invalid HTTP header");
			const auto line = name + (value.empty() ? ";" : ": " + value);
			auto* next = curl_slist_append(headers.get(), line.c_str());
			if (!next)
				throw std::bad_alloc();
			(void)headers.release();
			headers.reset(next);
		}
		// Reuse connections; serialize access to this easy handle. No application callback runs
		// here.
		std::lock_guard lock(mutex_);
		auto* handle = handle_.get();
		curl_easy_reset(handle);
		ResponseState state;
		// Drop borrowed request/response pointers before their buffers leave scope, including
		// on exceptions. Reset retains the connection cache for the next request.
		struct Reset {
			CURL* handle;
			~Reset() { curl_easy_reset(handle); }
		} reset{handle};
		Check(curl_easy_setopt(handle, CURLOPT_URL, request.url.c_str()));
		Check(curl_easy_setopt(handle, CURLOPT_PROTOCOLS_STR, "http,https"));
		Check(curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 0L));
		Check(curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L));
		Check(curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS,
		                       static_cast<long>(std::min<std::int64_t>(
		                           request.timeout.count(), std::numeric_limits<long>::max()))));
		Check(curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1L));
		Check(curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 2L));
		Check(curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers.get()));
		Check(curl_easy_setopt(handle, CURLOPT_POSTFIELDS, request.body.data()));
		Check(curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE_LARGE,
		                       static_cast<curl_off_t>(request.body.size())));
		Check(curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, request.method.c_str()));
		Check(curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, Body));
		Check(curl_easy_setopt(handle, CURLOPT_WRITEDATA, &state));
		Check(curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, Header));
		Check(curl_easy_setopt(handle, CURLOPT_HEADERDATA, &state));
		const auto code = curl_easy_perform(handle);
		if (state.failure)
			std::rethrow_exception(state.failure);
		Check(code);
		long status = 0;
		Check(curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status));
		state.response.status_code = static_cast<int>(status);
		return std::move(state.response);
	}

private:
	std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> handle_{nullptr, curl_easy_cleanup};
	std::mutex mutex_;
};
} // namespace

std::shared_ptr<HttpTransport> CreateCurlHttpTransport() {
	return std::make_shared<CurlTransport>();
}
} // namespace livekit::server
