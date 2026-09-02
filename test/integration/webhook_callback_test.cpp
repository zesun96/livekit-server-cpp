#include "livekit/server/livekit_api.h"
#include "livekit/server/webhook_receiver.h"
#include "livekit_room.pb.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace {

constexpr unsigned short webhook_port = 17890;

std::string Environment(const char* name) {
	char* value = nullptr;
	std::size_t size = 0;
	if (_dupenv_s(&value, &size, name) != 0 || value == nullptr) {
		return {};
	}
	std::string result(value);
	std::free(value);
	return result;
}

std::string Lower(std::string value) {
	std::transform(value.begin(), value.end(), value.begin(),
	               [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
	return value;
}

struct HttpWebhookRequest {
	std::string authorization;
	std::string body;
};

HttpWebhookRequest ReadRequest(SOCKET socket) {
	std::string data;
	std::size_t header_end = std::string::npos;
	std::size_t content_length = 0;
	for (;;) {
		char buffer[4096];
		const int received = recv(socket, buffer, sizeof(buffer), 0);
		if (received <= 0) {
			throw std::runtime_error("webhook HTTP connection closed early");
		}
		data.append(buffer, static_cast<std::size_t>(received));
		if (header_end == std::string::npos) {
			header_end = data.find("\r\n\r\n");
			if (header_end != std::string::npos) {
				std::size_t line_start = data.find("\r\n") + 2;
				while (line_start < header_end) {
					const auto line_end = data.find("\r\n", line_start);
					const auto colon = data.find(':', line_start);
					if (colon != std::string::npos && colon < line_end) {
						const auto name = Lower(data.substr(line_start, colon - line_start));
						if (name == "content-length") {
							content_length = static_cast<std::size_t>(
							    std::stoull(data.substr(colon + 1, line_end - colon - 1)));
						}
					}
					line_start = line_end + 2;
				}
			}
		}
		if (header_end != std::string::npos && data.size() >= header_end + 4 + content_length) {
			break;
		}
		if (data.size() > 1024 * 1024) {
			throw std::runtime_error("webhook HTTP request is unexpectedly large");
		}
	}

	HttpWebhookRequest request;
	std::size_t line_start = data.find("\r\n") + 2;
	while (line_start < header_end) {
		const auto line_end = data.find("\r\n", line_start);
		const auto colon = data.find(':', line_start);
		if (colon != std::string::npos && colon < line_end) {
			const auto name = Lower(data.substr(line_start, colon - line_start));
			if (name == "authorization") {
				std::size_t value_start = colon + 1;
				while (value_start < line_end && data[value_start] == ' ') {
					++value_start;
				}
				request.authorization = data.substr(value_start, line_end - value_start);
			}
		}
		line_start = line_end + 2;
	}
	request.body = data.substr(header_end + 4, content_length);
	return request;
}

void SendResponse(SOCKET socket, bool success) {
	const std::string response =
	    success ? "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"
	            : "HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\n"
	              "Connection: close\r\n\r\n";
	(void)send(socket, response.data(), static_cast<int>(response.size()), 0);
}

} // namespace

void RunWebhookCallbackIntegration() {
	const auto api_key = Environment("LIVEKIT_API_KEY");
	const auto api_secret = Environment("LIVEKIT_API_SECRET");

	WSADATA winsock{};
	if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0) {
		throw std::runtime_error("WSAStartup failed");
	}
	SOCKET listener = INVALID_SOCKET;
	std::thread server_thread;
	std::string room_name;
	try {
		listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (listener == INVALID_SOCKET) {
			throw std::runtime_error("failed to create webhook listener");
		}
		sockaddr_in address{};
		address.sin_family = AF_INET;
		address.sin_port = htons(webhook_port);
		InetPtonW(AF_INET, L"127.0.0.1", &address.sin_addr);
		if (bind(listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0 ||
		    listen(listener, 1) != 0) {
			throw std::runtime_error("failed to bind webhook listener on port 17890");
		}

		std::mutex event_mutex;
		std::condition_variable event_ready;
		bool received_room_started = false;
		std::exception_ptr thread_error;
		livekit::server::WebhookCallbacks callbacks;
		callbacks.on_room_started = [&](const livekit::server::WebhookEvent& event) {
			std::lock_guard lock(event_mutex);
			if (event.room && event.room->name == room_name) {
				received_room_started = true;
				event_ready.notify_one();
			}
		};
		livekit::server::WebhookReceiver receiver(api_key, api_secret, std::move(callbacks));
		server_thread = std::thread([&] {
			SOCKET connection = INVALID_SOCKET;
			try {
				connection = accept(listener, nullptr, nullptr);
				if (connection == INVALID_SOCKET) {
					throw std::runtime_error("failed to accept webhook connection");
				}
				const auto request = ReadRequest(connection);
				receiver.ReceiveAndDispatch(request.body, request.authorization);
				SendResponse(connection, true);
			} catch (...) {
				{
					std::lock_guard lock(event_mutex);
					thread_error = std::current_exception();
				}
				if (connection != INVALID_SOCKET) {
					SendResponse(connection, false);
				}
				event_ready.notify_one();
			}
			if (connection != INVALID_SOCKET) {
				closesocket(connection);
			}
		});

		room_name = "cpp-webhook-test-" +
		            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
		livekit::server::LiveKitApi api({});
		livekit::CreateRoomRequest create;
		create.set_name(room_name);
		(void)api.Room().CreateRoom(create);

		{
			std::unique_lock lock(event_mutex);
			if (!event_ready.wait_for(lock, std::chrono::seconds(10), [&] {
				    return received_room_started || thread_error != nullptr;
			    })) {
				throw std::runtime_error("timed out waiting for room_started webhook");
			}
		}
		if (thread_error != nullptr) {
			std::rethrow_exception(thread_error);
		}
		livekit::DeleteRoomRequest remove;
		remove.set_room(room_name);
		(void)api.Room().DeleteRoom(remove);
	} catch (...) {
		if (listener != INVALID_SOCKET) {
			closesocket(listener);
			listener = INVALID_SOCKET;
		}
		if (server_thread.joinable()) {
			server_thread.join();
		}
		WSACleanup();
		throw;
	}

	if (listener != INVALID_SOCKET) {
		closesocket(listener);
	}
	if (server_thread.joinable()) {
		server_thread.join();
	}
	WSACleanup();
}

TEST(WebhookIntegrationTest, ReceivesVerifiedRoomStartedCallback) {
	if (Environment("LIVEKIT_URL").empty() || Environment("LIVEKIT_API_KEY").empty() ||
	    Environment("LIVEKIT_API_SECRET").empty()) {
		GTEST_SKIP() << "set LIVEKIT_URL, LIVEKIT_API_KEY, and LIVEKIT_API_SECRET";
	}

	EXPECT_NO_THROW(RunWebhookCallbackIntegration());
}
