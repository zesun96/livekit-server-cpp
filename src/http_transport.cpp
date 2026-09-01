#include "livekit/server/http_transport.h"

#include "livekit/server/error.h"

#ifdef _WIN32
#include <windows.h>

#include <winhttp.h>
#endif

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace livekit::server {
namespace {

#ifdef _WIN32

std::wstring Utf8ToWide(const std::string& value) {
	if (value.empty()) {
		return {};
	}
	const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
	                                       static_cast<int>(value.size()), nullptr, 0);
	if (length <= 0) {
		throw Error(ErrorCode::invalid_argument, "HTTP value is not valid UTF-8");
	}
	std::wstring result(static_cast<std::size_t>(length), L'\0');
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
	                    result.data(), length);
	return result;
}

std::string WindowsMessage(const char* operation) {
	return std::string(operation) + " failed with Windows error " + std::to_string(GetLastError());
}

struct InternetHandleCloser {
	void operator()(void* handle) const noexcept {
		if (handle != nullptr) {
			WinHttpCloseHandle(static_cast<HINTERNET>(handle));
		}
	}
};

using InternetHandle = std::unique_ptr<void, InternetHandleCloser>;

class WinHttpTransport final : public HttpTransport {
public:
	WinHttpTransport()
	    : session_(WinHttpOpen(L"livekit-server-cpp/0.1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
	                           WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)) {
		if (session_ == nullptr) {
			throw Error(ErrorCode::transport, WindowsMessage("WinHttpOpen"));
		}
	}

	HttpResponse Send(const HttpRequest& request) override {
		auto url = Utf8ToWide(request.url);
		URL_COMPONENTS components{};
		components.dwStructSize = sizeof(components);
		components.dwSchemeLength = static_cast<DWORD>(-1);
		components.dwHostNameLength = static_cast<DWORD>(-1);
		components.dwUrlPathLength = static_cast<DWORD>(-1);
		components.dwExtraInfoLength = static_cast<DWORD>(-1);
		if (!WinHttpCrackUrl(url.data(), static_cast<DWORD>(url.size()), 0, &components)) {
			throw Error(ErrorCode::invalid_argument, WindowsMessage("WinHttpCrackUrl"));
		}
		if (components.nScheme != INTERNET_SCHEME_HTTP &&
		    components.nScheme != INTERNET_SCHEME_HTTPS) {
			throw Error(ErrorCode::invalid_argument, "HTTP transport requires an HTTP(S) URL");
		}

		const std::wstring host(components.lpszHostName, components.dwHostNameLength);
		std::wstring path(components.lpszUrlPath, components.dwUrlPathLength);
		if (components.dwExtraInfoLength != 0) {
			path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
		}
		if (path.empty()) {
			path = L"/";
		}
		const HINTERNET connection = Connection(host, components.nPort);
		const auto method = Utf8ToWide(request.method);
		InternetHandle request_handle(WinHttpOpenRequest(
		    connection, method.c_str(), path.c_str(), nullptr, WINHTTP_NO_REFERER,
		    WINHTTP_DEFAULT_ACCEPT_TYPES,
		    components.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0));
		if (request_handle == nullptr) {
			throw Error(ErrorCode::transport, WindowsMessage("WinHttpOpenRequest"));
		}

		const auto timeout = static_cast<int>(
		    std::min<std::int64_t>(request.timeout.count(), std::numeric_limits<int>::max()));
		if (!WinHttpSetTimeouts(request_handle.get(), timeout, timeout, timeout, timeout)) {
			throw Error(ErrorCode::transport, WindowsMessage("WinHttpSetTimeouts"));
		}
		std::string narrow_headers;
		for (const auto& [name, value] : request.headers) {
			narrow_headers += name + ": " + value + "\r\n";
		}
		const auto headers = Utf8ToWide(narrow_headers);
		void* body =
		    request.body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(request.body.data());
		if (!WinHttpSendRequest(request_handle.get(), headers.c_str(),
		                        static_cast<DWORD>(headers.size()), body,
		                        static_cast<DWORD>(request.body.size()),
		                        static_cast<DWORD>(request.body.size()), 0) ||
		    !WinHttpReceiveResponse(request_handle.get(), nullptr)) {
			throw Error(ErrorCode::transport, WindowsMessage("WinHTTP request"));
		}

		DWORD status = 0;
		DWORD status_size = sizeof(status);
		if (!WinHttpQueryHeaders(
		        request_handle.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
		        WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX)) {
			throw Error(ErrorCode::transport, WindowsMessage("WinHttpQueryHeaders"));
		}

		HttpResponse response;
		response.status_code = static_cast<int>(status);
		for (;;) {
			DWORD available = 0;
			if (!WinHttpQueryDataAvailable(request_handle.get(), &available)) {
				throw Error(ErrorCode::transport, WindowsMessage("WinHttpQueryDataAvailable"));
			}
			if (available == 0) {
				break;
			}
			const auto offset = response.body.size();
			response.body.resize(offset + available);
			DWORD received = 0;
			if (!WinHttpReadData(request_handle.get(), response.body.data() + offset, available,
			                     &received)) {
				throw Error(ErrorCode::transport, WindowsMessage("WinHttpReadData"));
			}
			response.body.resize(offset + received);
		}
		return response;
	}

	~WinHttpTransport() override {
		for (const auto& [key, connection] : connections_) {
			(void)key;
			WinHttpCloseHandle(connection);
		}
	}

private:
	HINTERNET Connection(const std::wstring& host, INTERNET_PORT port) {
		const auto key = host + L":" + std::to_wstring(port);
		std::lock_guard lock(connections_mutex_);
		const auto existing = connections_.find(key);
		if (existing != connections_.end()) {
			return existing->second;
		}
		const auto connection = WinHttpConnect(session_.get(), host.c_str(), port, 0);
		if (connection == nullptr) {
			throw Error(ErrorCode::transport, WindowsMessage("WinHttpConnect"));
		}
		connections_.emplace(key, connection);
		return connection;
	}

	InternetHandle session_;
	std::mutex connections_mutex_;
	std::map<std::wstring, HINTERNET> connections_;
};

#else

class UnsupportedHttpTransport final : public HttpTransport {
public:
	HttpResponse Send(const HttpRequest&) override {
		throw Error(ErrorCode::unsupported,
		            "no default HTTP transport is available on this platform");
	}
};

#endif

} // namespace

std::shared_ptr<HttpTransport> CreateDefaultHttpTransport() {
#ifdef _WIN32
	return std::make_shared<WinHttpTransport>();
#else
	return std::make_shared<UnsupportedHttpTransport>();
#endif
}

} // namespace livekit::server
