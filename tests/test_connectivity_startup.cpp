// Verifies access-point/station startup settings, ordering, and failures.
#include "test.hpp"

#include "firmware/application/connectivity_startup.hpp"

#include <string>
#include <vector>

using firmware::application::ConnectivityStartup;
using firmware::application::ConnectivityStartupConfig;
using firmware::application::ConnectivityStartupPort;
using firmware::application::StartupScanConfig;
using firmware::application::StartupScanOutcome;
using firmware::application::StartupScanState;
using firmware::application::WifiBand;

namespace {

// Records startup steps and returns programmable mode, scan, and AP outcomes.
class FakeConnectivityStartupPort final : public ConnectivityStartupPort {
public:
    // Enters station-only mode before congestion scanning.
    bool enter_station_only_mode() override {
        calls.emplace_back("station-only");
        return station_mode_succeeds;
    }

    // Returns the configured blocking scan outcome.
    StartupScanOutcome scan_channels(
        const StartupScanConfig& requested) override {
        calls.emplace_back("scan");
        scan_config = requested;
        return scan_outcome;
    }

    // Applies the complete concurrent AP/station configuration.
    bool start_access_point_and_station(
        const ConnectivityStartupConfig& requested) override {
        calls.emplace_back("ap-station");
        startup_config = requested;
        return ap_station_succeeds;
    }

    bool station_mode_succeeds = true;
    bool ap_station_succeeds = true;
    StartupScanOutcome scan_outcome{StartupScanState::success, {}};
    StartupScanConfig scan_config{};
    ConnectivityStartupConfig startup_config{};
    std::vector<std::string> calls;
};

}  // namespace

TEST_CASE(net_003_to_008_connectivity_starts_with_exact_scan_and_network_policy) {
    FakeConnectivityStartupPort port;
    port.scan_outcome = {StartupScanState::success, {1U, 1U, 3U, 14U}};

    const bool started = ConnectivityStartup::start(port, "machine");

    REQUIRE(started);
    REQUIRE_EQ(port.calls,
               std::vector<std::string>({"station-only", "scan", "ap-station"}));
    REQUIRE(port.scan_config.blocking);
    REQUIRE(port.scan_config.active);
    REQUIRE(port.scan_config.include_hidden);
    REQUIRE_EQ(port.scan_config.minimum_active_dwell_milliseconds, 100U);
    REQUIRE_EQ(port.scan_config.maximum_active_dwell_milliseconds, 300U);
    REQUIRE_EQ(port.startup_config.access_point_ssid, std::string("machine"));
    REQUIRE(port.startup_config.visible);
    REQUIRE(port.startup_config.open_authentication);
    REQUIRE_EQ(port.startup_config.band, WifiBand::ghz_2_4);
    REQUIRE_EQ(port.startup_config.maximum_clients, 4U);
    REQUIRE_EQ(port.startup_config.channel, 2U);
    REQUIRE(!port.startup_config.power_save_enabled);
    REQUIRE_EQ(port.startup_config.global_socket_capacity, 16U);
    REQUIRE_EQ(port.startup_config.default_hostname,
               std::string("Z1-Community"));
    REQUIRE_EQ(port.startup_config.access_point_ipv4,
               std::string("192.168.4.1"));
    REQUIRE_EQ(port.startup_config.access_point_prefix_length, 24U);
    REQUIRE(port.startup_config.access_point_dhcp_server);
    REQUIRE(port.startup_config.station_dhcp_client);
}

TEST_CASE(net_003_access_point_name_is_bounded_to_31_bytes) {
    FakeConnectivityStartupPort port;

    REQUIRE(ConnectivityStartup::start(port, std::string(40U, 'm')));
    REQUIRE_EQ(port.startup_config.access_point_ssid, std::string(31U, 'm'));
}

TEST_CASE(net_005_empty_or_unavailable_scan_storage_selects_channel_one) {
    FakeConnectivityStartupPort port;

    REQUIRE(ConnectivityStartup::start(port, "machine"));
    REQUIRE_EQ(port.startup_config.channel, 1U);

    port.scan_outcome = {StartupScanState::allocation_failure, {6U, 6U}};
    REQUIRE(ConnectivityStartup::start(port, "machine"));
    REQUIRE_EQ(port.startup_config.channel, 1U);
}

TEST_CASE(net_005_scan_api_failures_abort_connectivity_startup) {
    for (const StartupScanState failure : {
             StartupScanState::start_failure,
             StartupScanState::count_failure,
             StartupScanState::records_failure,
         }) {
        FakeConnectivityStartupPort port;
        port.scan_outcome = {failure, {}};

        REQUIRE(!ConnectivityStartup::start(port, "machine"));
        REQUIRE_EQ(port.calls,
                   std::vector<std::string>({"station-only", "scan"}));
    }
}

TEST_CASE(boot_015_required_connectivity_mode_failures_abort_startup) {
    FakeConnectivityStartupPort port;
    port.station_mode_succeeds = false;

    REQUIRE(!ConnectivityStartup::start(port, "machine"));
    REQUIRE_EQ(port.calls, std::vector<std::string>({"station-only"}));

    port = FakeConnectivityStartupPort{};
    port.ap_station_succeeds = false;
    REQUIRE(!ConnectivityStartup::start(port, "machine"));
    REQUIRE_EQ(port.calls,
               std::vector<std::string>({"station-only", "scan", "ap-station"}));
}
