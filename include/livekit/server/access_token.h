#pragma once

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace livekit::server {

struct VideoGrant {
	bool room_create{};
	bool room_list{};
	bool room_record{};
	bool room_admin{};
	bool room_join{};
	std::string room;
	std::string destination_room;
	bool ingress_admin{};
	std::optional<bool> can_publish;
	std::optional<bool> can_subscribe;
	std::optional<bool> can_publish_data;
	std::optional<bool> can_update_own_metadata;
	std::vector<std::string> can_publish_sources;
	bool hidden{};
	bool recorder{};
	bool agent{};
};

struct SipGrant {
	bool admin{};
	bool call{};
};

struct AgentGrant {
	bool admin{};
	bool simulation_admin{};
};

class AccessToken {
public:
	AccessToken(std::string api_key, std::string api_secret);

	AccessToken& SetIdentity(std::string identity);
	AccessToken& SetName(std::string name);
	AccessToken& SetMetadata(std::string metadata);
	AccessToken& SetAttributes(std::map<std::string, std::string> attributes);
	AccessToken& SetValidFor(std::chrono::seconds valid_for);
	AccessToken& SetVideoGrant(VideoGrant grant);
	AccessToken& SetSipGrant(SipGrant grant);
	AccessToken& SetAgentGrant(AgentGrant grant);

	[[nodiscard]] std::string ToJwt() const;
	[[nodiscard]] std::string ToJwt(std::chrono::system_clock::time_point now) const;

private:
	std::string api_key_;
	std::string api_secret_;
	std::string identity_;
	std::string name_;
	std::string metadata_;
	std::map<std::string, std::string> attributes_;
	std::chrono::seconds valid_for_{std::chrono::hours(6)};
	std::optional<VideoGrant> video_grant_;
	std::optional<SipGrant> sip_grant_;
	std::optional<AgentGrant> agent_grant_;
};

} // namespace livekit::server
