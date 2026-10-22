#include "version.hpp"

#include <charconv>
#include <system_error>

const std::string_view software_version = RUTILE_VERSION;
const std::uint32_t protocol_version = RUTILE_PROTOCOL_VERSION;

std::string version_handshake() {
	return "rutile " + std::to_string(protocol_version) + " " + std::string(software_version);
}

std::optional<struct PeerVersion> version_parse(std::string_view response) {
	if (!response.starts_with("rutile ")) {
		return std::nullopt;
	}
	response.remove_prefix(7);
	std::size_t separator = response.find(' ');
	if (separator == std::string_view::npos || !separator) {
		return std::nullopt;
	}
	std::uint32_t protocol{};
	auto result = std::from_chars(response.data(), response.data() + separator, protocol);
	if (result.ec != std::errc{} || result.ptr != response.data() + separator || !protocol) {
		return std::nullopt;
	}
	std::string_view release = response.substr(separator + 1);
	if (release.empty() || release.find_first_of(" \t\n\r\f\v") != std::string_view::npos) {
		return std::nullopt;
	}
	struct PeerVersion peer{ protocol, std::string(release) };
	return peer;
}

bool version_compatible(const struct PeerVersion& peer) {
	return peer.protocol == protocol_version;
}
