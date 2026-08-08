// Verifies BLE provisioning lifecycle, security gates, and Wi-Fi commands.
#include "test.hpp"

#include "firmware/application/ble_provisioning.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using firmware::application::BleLifecycleConfig;
using firmware::application::BleProvisioning;
using firmware::application::BleProvisioningPort;
using firmware::application::BleStationReportState;
using firmware::application::BleWifiListEntry;
using firmware::application::BleWifiScanOutcome;
using firmware::application::BleWifiStatusReport;
using firmware::application::StationApiResult;
using firmware::application::StationConfiguration;
using firmware::application::StationConnectionState;
using firmware::application::StationRuntime;
using firmware::application::WifiScanConfig;
using firmware::core::ByteVector;

namespace {

// Converts test text to the byte-oriented provisioning input surface.
ByteVector bytes(std::string_view value) {
    return {value.begin(), value.end()};
}

// Records all lifecycle, station, scan, report, and diagnostic operations.
class FakeBleProvisioningPort final : public BleProvisioningPort {
public:
    // Records provisioning initialization and returns its configured outcome.
    bool initialize(const BleLifecycleConfig& requested) override {
        lifecycle_config = requested;
        calls.emplace_back("initialize");
        return initialization_succeeds;
    }

    // Starts advertising under the exact requested device name.
    bool start_advertising(std::string_view device_name) override {
        calls.emplace_back("advertise");
        advertised_name = std::string(device_name);
        return advertising_succeeds;
    }

    // Stops advertising when a client connects.
    void stop_advertising() override {
        calls.emplace_back("stop-advertising");
    }

    // Creates a fresh per-connection security context.
    void create_security_context() override {
        calls.emplace_back("create-security");
    }

    // Destroys the disconnected client's security context.
    void destroy_security_context() override {
        calls.emplace_back("destroy-security");
    }

    // Applies the fast-scan station configuration.
    StationApiResult apply_station_config(
        const StationConfiguration& requested) override {
        station_config = requested;
        calls.emplace_back("configure-station");
        return {true, {}};
    }

    // Requests station disconnection without mutating persistent credentials.
    StationApiResult request_station_disconnect() override {
        calls.emplace_back("disconnect-station");
        return {true, {}};
    }

    // Requests station association without a settling delay.
    StationApiResult request_station_connect() override {
        calls.emplace_back("connect-station");
        return {true, {}};
    }

    // Returns the adapter's current numeric Wi-Fi mode.
    std::uint8_t current_wifi_mode() const override {
        return wifi_mode;
    }

    // Stops a prior scan before a provisioning list request.
    void stop_scan() override {
        calls.emplace_back("stop-scan");
    }

    // Records the scan settling delay.
    void delay_milliseconds(std::uint32_t duration) override {
        delays.push_back(duration);
    }

    // Returns the configured scan result and captures exact scan settings.
    BleWifiScanOutcome scan(const WifiScanConfig& requested) override {
        scan_config = requested;
        calls.emplace_back("scan");
        return scan_outcome;
    }

    // Reports one BLUFI protocol error byte.
    void report_error(std::uint8_t error) override {
        errors.push_back(error);
    }

    // Sends one BLUFI Wi-Fi status report.
    void send_wifi_status(const BleWifiStatusReport& report) override {
        status_reports.push_back(report);
    }

    // Sends one nonempty BLUFI Wi-Fi list.
    void send_wifi_list(const std::vector<BleWifiListEntry>& entries) override {
        wifi_lists.push_back(entries);
    }

    // Records accepted custom data for diagnostic output.
    void log_custom_data(firmware::core::BytesView data) override {
        custom_data.emplace_back(data.begin(), data.end());
    }

    bool initialization_succeeds = true;
    bool advertising_succeeds = true;
    std::uint8_t wifi_mode = 3U;
    BleLifecycleConfig lifecycle_config{};
    StationConfiguration station_config{};
    BleWifiScanOutcome scan_outcome{true, true, {}};
    WifiScanConfig scan_config{};
    std::string advertised_name;
    std::vector<std::string> calls;
    std::vector<std::uint32_t> delays;
    std::vector<std::uint8_t> errors;
    std::vector<BleWifiStatusReport> status_reports;
    std::vector<std::vector<BleWifiListEntry>> wifi_lists;
    std::vector<ByteVector> custom_data;
};

}  // namespace

TEST_CASE(ble_001_to_005_startup_uses_exact_identity_and_security_policy) {
    StationRuntime runtime;
    FakeBleProvisioningPort port;
    BleProvisioning provisioning(runtime, port, "Makera_Z1_1234");

    REQUIRE(provisioning.start());
    REQUIRE(port.lifecycle_config.standard_blufi_service);
    REQUIRE(port.lifecycle_config.low_energy_only);
    REQUIRE(!port.lifecycle_config.link_pairing_enabled);
    REQUIRE(!port.lifecycle_config.security_manager_authentication);
    REQUIRE_EQ(port.advertised_name, std::string("MK_Makera_Z1_1234"));

    FakeBleProvisioningPort failed_port;
    failed_port.initialization_succeeds = false;
    BleProvisioning optional_service(runtime, failed_port, "Makera_Z1_1234");
    REQUIRE(!optional_service.start());
    REQUIRE_EQ(failed_port.calls, std::vector<std::string>({"initialize"}));
}

TEST_CASE(ble_003_and_006_connections_replace_security_but_retain_credentials) {
    StationRuntime runtime;
    runtime.ssid = "retained";
    runtime.password = "secret";
    FakeBleProvisioningPort port;
    BleProvisioning provisioning(runtime, port, "Makera_Z1_1234");
    REQUIRE(provisioning.start());

    provisioning.client_connected();
    provisioning.security_negotiated();
    REQUIRE(provisioning.security_ready());
    provisioning.client_disconnected();

    REQUIRE(!provisioning.security_ready());
    REQUIRE_EQ(runtime.ssid, std::string("retained"));
    REQUIRE_EQ(runtime.password, std::string("secret"));
    REQUIRE_EQ(port.calls,
               std::vector<std::string>({"initialize", "advertise",
                                         "stop-advertising", "create-security",
                                         "destroy-security", "advertise"}));
}

TEST_CASE(ble_010_credentials_and_connect_are_ignored_before_security) {
    StationRuntime runtime;
    FakeBleProvisioningPort port;
    BleProvisioning provisioning(runtime, port, "Makera_Z1_1234");

    provisioning.receive_ssid(bytes("ap"));
    provisioning.receive_password(bytes("pw"));
    provisioning.connect_station();

    REQUIRE(runtime.ssid.empty());
    REQUIRE(runtime.password.empty());
    REQUIRE(port.errors.empty());
    REQUIRE(port.calls.empty());
}

TEST_CASE(ble_010_credentials_validate_raw_length_then_decode_escapes) {
    StationRuntime runtime;
    FakeBleProvisioningPort port;
    BleProvisioning provisioning(runtime, port, "Makera_Z1_1234");
    provisioning.client_connected();
    provisioning.security_negotiated();

    const ByteVector escaped_ssid{'m', 'y', 0x01U, 'a', 'p', 0U, 'x'};
    const ByteVector escaped_password{'p', 0x02U, 's', 's'};
    provisioning.receive_ssid(escaped_ssid);
    provisioning.receive_password(escaped_password);
    REQUIRE_EQ(runtime.ssid, std::string("my ap"));
    REQUIRE_EQ(runtime.password, std::string("p?ss"));

    provisioning.receive_ssid(ByteVector(32U, 's'));
    provisioning.receive_password(ByteVector(64U, 'p'));
    REQUIRE_EQ(port.errors, std::vector<std::uint8_t>({9U, 9U}));
    REQUIRE_EQ(runtime.ssid, std::string("my ap"));
    REQUIRE_EQ(runtime.password, std::string("p?ss"));
}

TEST_CASE(ble_011_secure_connect_uses_fast_scan_without_manual_attempt_state) {
    StationRuntime runtime;
    runtime.automatic_retry_number = 3U;
    FakeBleProvisioningPort port;
    BleProvisioning provisioning(runtime, port, "Makera_Z1_1234");
    provisioning.client_connected();
    provisioning.security_negotiated();
    provisioning.receive_ssid(bytes("ap"));
    provisioning.receive_password(bytes("pw"));

    provisioning.connect_station();

    REQUIRE(port.station_config.fast_scan);
    REQUIRE(!port.station_config.scan_all_channels);
    REQUIRE_EQ(port.station_config.ssid, std::string("ap"));
    REQUIRE_EQ(port.station_config.password, std::string("pw"));
    REQUIRE_EQ(port.calls[2], std::string("configure-station"));
    REQUIRE_EQ(port.calls[3], std::string("disconnect-station"));
    REQUIRE_EQ(port.calls[4], std::string("connect-station"));
    REQUIRE_EQ(runtime.state, StationConnectionState::idle);
    REQUIRE_EQ(runtime.automatic_retry_number, 3U);
    REQUIRE(port.delays.empty());
}

TEST_CASE(ble_011_missing_ssid_reports_data_format_error) {
    StationRuntime runtime;
    FakeBleProvisioningPort port;
    BleProvisioning provisioning(runtime, port, "Makera_Z1_1234");
    provisioning.client_connected();
    provisioning.security_negotiated();

    provisioning.connect_station();

    REQUIRE_EQ(port.errors, std::vector<std::uint8_t>({9U}));
}

TEST_CASE(ble_012_disconnect_and_operation_mode_do_not_clear_credentials) {
    StationRuntime runtime;
    runtime.ssid = "ap";
    runtime.password = "pw";
    FakeBleProvisioningPort port;
    BleProvisioning provisioning(runtime, port, "Makera_Z1_1234");

    provisioning.disconnect_station();
    provisioning.set_operation_mode(1U);

    REQUIRE_EQ(port.calls,
               std::vector<std::string>({"disconnect-station"}));
    REQUIRE_EQ(runtime.ssid, std::string("ap"));
    REQUIRE_EQ(runtime.password, std::string("pw"));
}

TEST_CASE(ble_013_status_uses_mode_runtime_state_bssid_and_ssid) {
    StationRuntime runtime;
    runtime.ssid = "ap";
    FakeBleProvisioningPort port;
    BleProvisioning provisioning(runtime, port, "Makera_Z1_1234");
    const std::array<std::uint8_t, 6U> bssid{1U, 2U, 3U, 4U, 5U, 6U};
    provisioning.station_associated(bssid, "reported-ap");

    provisioning.request_status();
    REQUIRE_EQ(port.status_reports.back().station_state,
               BleStationReportState::failure);
    REQUIRE_EQ(port.status_reports.back().wifi_mode, 3U);
    REQUIRE_EQ(port.status_reports.back().bssid, bssid);
    REQUIRE_EQ(port.status_reports.back().ssid, std::string("reported-ap"));

    runtime.state = StationConnectionState::attempt_started;
    provisioning.request_status();
    REQUIRE_EQ(port.status_reports.back().station_state,
               BleStationReportState::connecting);

    runtime.state = StationConnectionState::address_ready;
    runtime.error_detail.clear();
    provisioning.request_status();
    REQUIRE_EQ(port.status_reports.back().station_state,
               BleStationReportState::success);
}

TEST_CASE(ble_014_ipv4_event_reports_only_to_connected_ble_client) {
    StationRuntime runtime;
    runtime.password = "pw";
    FakeBleProvisioningPort port;
    BleProvisioning provisioning(runtime, port, "Makera_Z1_1234");
    const std::array<std::uint8_t, 6U> bssid{6U, 5U, 4U, 3U, 2U, 1U};
    provisioning.station_associated(bssid, "ap");

    provisioning.station_address_ready("192.0.2.40");
    REQUIRE(port.status_reports.empty());

    provisioning.client_connected();
    provisioning.security_negotiated();
    provisioning.receive_ssid(bytes("later-staged"));
    provisioning.station_address_ready("192.0.2.41");
    REQUIRE_EQ(port.status_reports.size(), 1U);
    REQUIRE_EQ(port.status_reports[0].station_state,
               BleStationReportState::success);
    REQUIRE_EQ(port.status_reports[0].ssid, std::string("ap"));
    provisioning.station_disconnected();
    provisioning.request_status();
    const std::array<std::uint8_t, 6U> empty_bssid{};
    REQUIRE_EQ(port.status_reports.back().bssid, empty_bssid);
    REQUIRE_EQ(runtime.ssid, std::string("later-staged"));
    REQUIRE_EQ(runtime.password, std::string("pw"));
}

TEST_CASE(ble_015_wifi_list_uses_user_scan_policy_and_omits_empty_results) {
    StationRuntime runtime;
    FakeBleProvisioningPort port;
    BleProvisioning provisioning(runtime, port, "Makera_Z1_1234");
    port.scan_outcome.observations = {
        {bytes("weak"), -80, 1U},
        {bytes("strong"), -30, 0U},
        {bytes("weak"), -70, 1U},
    };

    provisioning.request_wifi_list();

    REQUIRE_EQ(port.delays, std::vector<std::uint32_t>({100U}));
    REQUIRE(port.scan_config.active);
    REQUIRE(port.scan_config.include_hidden);
    REQUIRE_EQ(port.scan_config.active_dwell_milliseconds, 120U);
    REQUIRE_EQ(port.scan_config.passive_dwell_milliseconds, 360U);
    REQUIRE_EQ(port.scan_config.maximum_observations, 20U);
    REQUIRE_EQ(port.wifi_lists.size(), 1U);
    REQUIRE_EQ(port.wifi_lists[0][0].ssid, std::string("strong"));
    REQUIRE_EQ(port.wifi_lists[0][0].rssi, -30);
    REQUIRE_EQ(port.wifi_lists[0][1].ssid, std::string("weak"));
    REQUIRE_EQ(port.wifi_lists[0][1].rssi, -70);

    port.scan_outcome.observations.clear();
    provisioning.request_wifi_list();
    REQUIRE_EQ(port.wifi_lists.size(), 1U);

    port.scan_outcome.success = false;
    provisioning.request_wifi_list();
    port.scan_outcome = {true, false, {}};
    provisioning.request_wifi_list();
    REQUIRE_EQ(port.errors,
               std::vector<std::uint8_t>({11U, 11U}));
}

TEST_CASE(ble_016_error_is_echoed_and_custom_data_only_reaches_diagnostics) {
    StationRuntime runtime;
    FakeBleProvisioningPort port;
    BleProvisioning provisioning(runtime, port, "Makera_Z1_1234");

    provisioning.receive_error(7U);
    provisioning.receive_custom_data(bytes("diagnostic"));

    REQUIRE_EQ(port.errors, std::vector<std::uint8_t>({7U}));
    REQUIRE_EQ(port.custom_data,
               std::vector<ByteVector>({bytes("diagnostic")}));
    REQUIRE(port.status_reports.empty());
    REQUIRE(port.wifi_lists.empty());
}
