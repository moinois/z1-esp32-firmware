/** @file @brief Implements exact station-scan then concurrent AP/station startup policy. */
#include "application/connectivity/connectivity_startup.hpp"

#include "application/connectivity/connectivity_defaults.hpp"
#include "core/network/network_policy.hpp"

#include <cstddef>
#include <limits>
#include <string>
#include <string_view>

namespace firmware::application {
namespace {

constexpr std::size_t maximum_access_point_name_size = 31U;
constexpr std::uint16_t minimum_active_dwell_milliseconds = 100U;
constexpr std::uint16_t maximum_active_dwell_milliseconds = 300U;

constexpr StartupScanConfig startup_scan_config{
    true,
    true,
    true,
    minimum_active_dwell_milliseconds,
    maximum_active_dwell_milliseconds,
};

// Reports whether a scan failure must abort required connectivity startup.
bool fatal_scan_failure(StartupScanState state) {
    return state == StartupScanState::start_failure ||
           state == StartupScanState::count_failure ||
           state == StartupScanState::records_failure;
}

std::optional<std::uint8_t> initial_saved_channel(std::string_view text) {
    if (text.empty() || text.front() < '0' || text.front() > '9') {
        return std::nullopt;
    }
    unsigned value = 0U;
    for (const char character : text) {
        if (character < '0' || character > '9') break;
        const unsigned digit = static_cast<unsigned>(character - '0');
        if (value > (std::numeric_limits<unsigned>::max() - digit) / 10U) {
            return std::nullopt;
        }
        value = value * 10U + digit;
    }
    return value >= 1U && value <= 11U
               ? std::optional<std::uint8_t>(static_cast<std::uint8_t>(value))
               : std::nullopt;
}

}  // namespace

AccessPointStartupSettings parse_access_point_startup_settings(
    std::optional<std::string_view> channel,
    std::optional<std::uint8_t> legacy_channel,
    std::optional<std::string_view> password,
    std::optional<std::uint8_t> enabled) {
    AccessPointStartupSettings settings;
    if (channel.has_value()) {
        settings.saved_channel = initial_saved_channel(*channel);
    } else if (legacy_channel.has_value() && *legacy_channel >= 1U &&
               *legacy_channel <= 11U) {
        settings.saved_channel = legacy_channel;
    }
    settings.password = password.has_value() ? std::string(*password) : std::string{};
    settings.enabled = !enabled.has_value() || *enabled != 0U;
    return settings;
}

bool ConnectivityStartup::start(ConnectivityStartupPort& port,
                                std::string_view machine_name,
                                const AccessPointStartupSettings& settings) {
    if (!port.enter_station_only_mode()) {
        return false;
    }
    const StartupScanOutcome scan = port.scan_channels(startup_scan_config);
    if (fatal_scan_failure(scan.state)) {
        return false;
    }

    const bool result_storage_available =
        scan.state != StartupScanState::allocation_failure;
    if (!settings.enabled) {
        return port.finish_station_only_mode();
    }
    ConnectivityStartupConfig config;
    config.access_point_ssid = std::string(
        machine_name.substr(0U, maximum_access_point_name_size));
    config.visible = true;
    config.open_authentication = settings.password.empty();
    config.password = settings.password;
    config.band = WifiBand::ghz_2_4;
    config.maximum_clients =
        connectivity_defaults::maximum_access_point_clients;
    config.channel = settings.saved_channel.value_or(
        core::select_access_point_channel(scan.observed_channels,
                                          result_storage_available));
    config.power_save_enabled = false;
    config.global_socket_capacity = connectivity_defaults::global_socket_capacity;
    config.default_hostname =
        std::string(connectivity_defaults::network_hostname);
    config.access_point_ipv4 =
        std::string(connectivity_defaults::access_point_ipv4);
    config.access_point_prefix_length =
        connectivity_defaults::access_point_prefix_length;
    config.access_point_dhcp_server = true;
    config.station_dhcp_client = true;
    return port.start_access_point_and_station(config);
}

}  // namespace firmware::application
