#pragma once

#include <string>
#include <string_view>
#include <utility>

namespace livekit::server::model {

// An SDK-owned representation of a LiveKit protocol message. The JSON payload follows the
// canonical protobuf JSON mapping, while keeping generated protobuf types out of public APIs.
template <typename Tag> class ProtocolModel {
public:
	ProtocolModel() = default;
	explicit ProtocolModel(std::string json) : json_(std::move(json)) {}

	[[nodiscard]] static ProtocolModel FromJson(std::string json) {
		return ProtocolModel(std::move(json));
	}

	[[nodiscard]] std::string_view Json() const noexcept { return json_; }
	[[nodiscard]] bool Empty() const noexcept { return json_.empty() || json_ == "{}"; }

	friend bool operator==(const ProtocolModel&, const ProtocolModel&) = default;

private:
	std::string json_{"{}"};
};

} // namespace livekit::server::model
