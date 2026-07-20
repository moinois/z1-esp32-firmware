// Declares deterministic UDP discovery state, payload, and IPv4 policy.
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace firmware::core {

// Extracts the bounded state field or the specified absent/malformed fallback.
std::string discovery_machine_state(
    std::optional<std::string_view> controller_status);

// Formats and bounds one discovery datagram payload without a line ending.
std::string format_discovery_payload(std::string_view machine_name,
                                     std::string_view interface_ipv4,
                                     std::size_t active_tcp_clients,
                                     std::string_view machine_state);

// Computes an IPv4 subnet broadcast address from dotted address and netmask.
std::optional<std::string> ipv4_broadcast_address(std::string_view ipv4,
                                                  std::string_view netmask);

}  // namespace firmware::core
