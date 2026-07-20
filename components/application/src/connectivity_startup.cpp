// Implements exact station-scan then concurrent AP/station startup policy.
#include "firmware/application/connectivity_startup.hpp"

#include "firmware/application/connectivity_defaults.hpp"
#include "firmware/core/network_policy.hpp"

#include <cstddef>
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

}  // namespace

bool ConnectivityStartup::start(ConnectivityStartupPort& port,
                                std::string_view machine_name) {
    if (!port.enter_station_only_mode()) {
        return false;
    }
    const StartupScanOutcome scan = port.scan_channels(startup_scan_config);
    if (fatal_scan_failure(scan.state)) {
        return false;
    }

    const bool result_storage_available =
        scan.state != StartupScanState::allocation_failure;
    ConnectivityStartupConfig config;
    config.access_point_ssid = std::string(
        machine_name.substr(0U, maximum_access_point_name_size));
    config.visible = true;
    config.open_authentication = true;
    config.band = WifiBand::ghz_2_4;
    config.maximum_clients =
        connectivity_defaults::maximum_access_point_clients;
    config.channel = core::select_access_point_channel(
        scan.observed_channels, result_storage_available);
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
