#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

extern const std::string_view software_version;
extern const std::uint32_t protocol_version;

struct PeerVersion {
	std::uint32_t protocol;
	std::string release;
};

std::string version_handshake();
std::optional<struct PeerVersion> version_parse(std::string_view response);
bool version_compatible(const struct PeerVersion& peer);
