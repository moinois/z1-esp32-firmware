// Implements bounded discovery formatting and dependency-free IPv4 parsing.
#include "firmware/core/discovery_policy.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

namespace firmware::core {
namespace {

constexpr std::size_t maximum_machine_state_size = 31U;
constexpr std::size_t maximum_discovery_payload_size = 127U;
constexpr std::size_t discovery_tcp_capacity = 4U;
constexpr std::string_view absent_machine_state = "Idle";
constexpr std::string_view malformed_machine_state = "Unknown";
constexpr std::string_view controller_tcp_port = "2222";

// Parses exactly four decimal IPv4 octets without platform networking APIs.
std::optional<std::uint32_t> parse_ipv4(std::string_view text) {
    std::array<std::uint32_t, 4U> octets{};
    std::size_t cursor = 0U;
    for (std::size_t index = 0U; index < octets.size(); ++index) {
        if (cursor == text.size()) {
            return std::nullopt;
        }
        std::uint32_t value = 0U;
        std::size_t digits = 0U;
        while (cursor < text.size() && text[cursor] >= '0' &&
               text[cursor] <= '9') {
            value = value * 10U +
                    static_cast<std::uint32_t>(text[cursor] - '0');
            if (value > 255U) {
                return std::nullopt;
            }
            ++cursor;
            ++digits;
        }
        if (digits == 0U) {
            return std::nullopt;
        }
        octets[index] = value;
        if (index + 1U < octets.size()) {
            if (cursor == text.size() || text[cursor] != '.') {
                return std::nullopt;
            }
            ++cursor;
        }
    }
    if (cursor != text.size()) {
        return std::nullopt;
    }
    return (octets[0] << 24U) | (octets[1] << 16U) | (octets[2] << 8U) |
           octets[3];
}

// Formats a host-order IPv4 value as dotted decimal.
std::string format_ipv4(std::uint32_t address) {
    char text[16];
    const int length = std::snprintf(
        text, sizeof(text), "%u.%u.%u.%u",
        static_cast<unsigned>((address >> 24U) & 0xFFU),
        static_cast<unsigned>((address >> 16U) & 0xFFU),
        static_cast<unsigned>((address >> 8U) & 0xFFU),
        static_cast<unsigned>(address & 0xFFU));
    if (length < 0 || static_cast<std::size_t>(length) >= sizeof(text)) {
        return {};
    }
    return {text, static_cast<std::size_t>(length)};
}

}  // namespace

std::string discovery_machine_state(
    std::optional<std::string_view> controller_status) {
    if (!controller_status.has_value()) {
        return std::string(absent_machine_state);
    }
    const std::size_t opening = controller_status->find('<');
    if (opening == std::string_view::npos) {
        return std::string(malformed_machine_state);
    }
    const std::size_t separator = controller_status->find('|', opening + 1U);
    if (separator == std::string_view::npos) {
        return std::string(malformed_machine_state);
    }
    return std::string(controller_status->substr(
        opening + 1U,
        std::min(separator - opening - 1U, maximum_machine_state_size)));
}

std::string format_discovery_payload(std::string_view machine_name,
                                     std::string_view interface_ipv4,
                                     std::size_t active_tcp_clients,
                                     std::string_view machine_state) {
    std::string payload;
    payload.reserve(maximum_discovery_payload_size);
    payload.append(machine_name);
    payload.push_back(',');
    payload.append(interface_ipv4);
    payload.push_back(',');
    payload.append(controller_tcp_port);
    payload.push_back(',');
    payload.push_back(active_tcp_clients == discovery_tcp_capacity ? '1' : '0');
    payload.push_back(',');
    payload.append(machine_state);
    payload.resize(std::min(payload.size(), maximum_discovery_payload_size));
    return payload;
}

std::optional<std::string> ipv4_broadcast_address(std::string_view ipv4,
                                                  std::string_view netmask) {
    const auto address = parse_ipv4(ipv4);
    const auto mask = parse_ipv4(netmask);
    if (!address.has_value() || !mask.has_value()) {
        return std::nullopt;
    }
    return format_ipv4(*address | ~*mask);
}

}  // namespace firmware::core
