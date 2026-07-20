// Implements discovery socket lifecycle, periodic sends, and temporary bursts.
#include "firmware/application/discovery_service.hpp"

#include "firmware/core/discovery_policy.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace firmware::application {
namespace {

constexpr std::uint16_t discovery_port = 3333U;
constexpr std::string_view access_point_ipv4 = "192.168.4.1";
constexpr std::string_view access_point_broadcast = "192.168.4.255";
constexpr std::uint32_t periodic_interval_milliseconds = 500U;
constexpr std::uint32_t socket_retry_milliseconds = 2000U;
constexpr std::size_t burst_copy_count = 3U;
constexpr std::uint32_t burst_copy_delay_milliseconds = 100U;

}  // namespace

DiscoveryService::DiscoveryService(DiscoveryPort& port, std::string machine_name)
    : port_(port), machine_name_(std::move(machine_name)) {}

void DiscoveryService::periodic_cycle(
    std::optional<std::string_view> controller_status,
    std::size_t active_tcp_clients) {
    if (recreate_long_socket_) {
        if (long_socket_open_) {
            port_.close_long_lived_socket();
        }
        long_socket_open_ = false;
        recreate_long_socket_ = false;
    }
    if (!long_socket_open_) {
        if (!port_.open_long_lived_socket()) {
            port_.delay_milliseconds(socket_retry_milliseconds);
            return;
        }
        long_socket_open_ = true;
        static_cast<void>(port_.enable_long_lived_broadcast());
    }

    const std::string state =
        core::discovery_machine_state(controller_status);
    if (!station_ipv4_.empty() && !station_broadcast_.empty()) {
        const std::string station_payload = core::format_discovery_payload(
            machine_name_, station_ipv4_, active_tcp_clients, state);
        port_.send_long_lived(station_broadcast_, discovery_port,
                              station_payload);
    }
    const std::string access_point_payload = core::format_discovery_payload(
        machine_name_, access_point_ipv4, active_tcp_clients, state);
    port_.send_long_lived(access_point_broadcast, discovery_port,
                          access_point_payload);
    port_.delay_milliseconds(periodic_interval_milliseconds);
}

void DiscoveryService::station_address_assigned(
    std::string_view ipv4, std::string_view netmask,
    std::optional<std::string_view> controller_status,
    std::size_t active_tcp_clients) {
    station_ipv4_ = std::string(ipv4);
    const auto broadcast = core::ipv4_broadcast_address(ipv4, netmask);
    station_broadcast_ = broadcast.has_value() ? *broadcast : std::string{};
    recreate_long_socket_ = true;
    send_temporary_burst(controller_status, active_tcp_clients, false);
}

void DiscoveryService::station_disconnected() {
    station_ipv4_.clear();
    station_broadcast_.clear();
    recreate_long_socket_ = true;
}

void DiscoveryService::send_command_burst(
    std::optional<std::string_view> controller_status,
    std::size_t active_tcp_clients) {
    send_temporary_burst(controller_status, active_tcp_clients, true);
}

void DiscoveryService::send_temporary_burst(
    std::optional<std::string_view> controller_status,
    std::size_t active_tcp_clients, bool delay_after_each_copy) {
    if (station_ipv4_.empty() || station_broadcast_.empty() ||
        !port_.open_temporary_socket()) {
        return;
    }
    const std::string state =
        core::discovery_machine_state(controller_status);
    const std::string payload = core::format_discovery_payload(
        machine_name_, station_ipv4_, active_tcp_clients, state);
    for (std::size_t copy = 0U; copy < burst_copy_count; ++copy) {
        port_.send_temporary(station_broadcast_, discovery_port, payload);
        if (delay_after_each_copy) {
            port_.delay_milliseconds(burst_copy_delay_milliseconds);
        }
    }
    port_.close_temporary_socket();
}

}  // namespace firmware::application
