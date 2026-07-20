// Verifies runtime station state and manual connect/disconnect policy.
#include "test.hpp"

#include "firmware/application/station_connection.hpp"
#include "firmware/application/wlan_command.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using firmware::application::ManualStationConnection;
using firmware::application::StationApiResult;
using firmware::application::StationConnectionPort;
using firmware::application::StationConnectionState;
using firmware::application::StationRuntime;
using firmware::application::StationSnapshot;
using firmware::application::WlanConnectionCommand;
using firmware::application::WlanConnectionResponsePort;
using firmware::core::ByteVector;
using firmware::core::Frame;

namespace {

// Records station API calls and returns a programmable snapshot sequence.
class FakeStationConnectionPort final : public StationConnectionPort {
public:
    // Records a station disconnect request.
    StationApiResult request_disconnect() override {
        calls.emplace_back("disconnect");
        return disconnect_result;
    }

    // Records the exact credentials and protected-management setting.
    StationApiResult apply_station_config(std::string_view requested_ssid,
                                          std::string_view requested_password,
                                          bool optional_pmf) override {
        calls.emplace_back("configure");
        ssid = std::string(requested_ssid);
        password = std::string(requested_password);
        protected_management_optional = optional_pmf;
        return configure_result;
    }

    // Records an association request.
    StationApiResult request_connect() override {
        calls.emplace_back("connect");
        return connect_result;
    }

    // Records polling delays without sleeping the host test.
    void delay_milliseconds(std::uint32_t duration) override {
        delays.push_back(duration);
    }

    // Returns successive asynchronous station snapshots.
    StationSnapshot station_snapshot() const override {
        if (snapshots.empty()) {
            return {};
        }
        const std::size_t selected =
            snapshot_index < snapshots.size() ? snapshot_index
                                              : snapshots.size() - 1U;
        ++snapshot_index;
        return snapshots[selected];
    }

    // Records the credentials persisted after address assignment.
    StationApiResult save_credentials(std::string_view saved_ssid,
                                      std::string_view saved_password) override {
        calls.emplace_back("save");
        persisted_ssid = std::string(saved_ssid);
        persisted_password = std::string(saved_password);
        return save_result;
    }

    StationApiResult disconnect_result{true, {}};
    StationApiResult configure_result{true, {}};
    StationApiResult connect_result{true, {}};
    StationApiResult save_result{true, {}};
    mutable std::size_t snapshot_index = 0U;
    std::vector<StationSnapshot> snapshots;
    std::vector<std::string> calls;
    std::vector<std::uint32_t> delays;
    std::string ssid;
    std::string password;
    std::string persisted_ssid;
    std::string persisted_password;
    bool protected_management_optional = false;
};

// Records host frames, the discovery delay, and the discovery burst.
class FakeWlanConnectionResponsePort final
    : public WlanConnectionResponsePort {
public:
    // Records a response frame in send order.
    void send(Frame frame) override {
        sent.push_back(std::move(frame));
    }

    // Records the post-connection discovery delay.
    void delay_milliseconds(std::uint32_t duration) override {
        delay = duration;
    }

    // Records one command-triggered discovery burst.
    void send_discovery_burst() override {
        discovery_sent = true;
    }

    std::vector<Frame> sent;
    std::uint32_t delay = 0U;
    bool discovery_sent = false;
};

// Converts response payload bytes to text for exact protocol assertions.
std::string text(const ByteVector& payload) {
    return {payload.begin(), payload.end()};
}

}  // namespace

TEST_CASE(net_020_station_connection_states_have_exact_numeric_values) {
    REQUIRE_EQ(static_cast<std::uint8_t>(StationConnectionState::idle), 0U);
    REQUIRE_EQ(static_cast<std::uint8_t>(StationConnectionState::attempt_started),
               1U);
    REQUIRE_EQ(static_cast<std::uint8_t>(StationConnectionState::associated), 2U);
    REQUIRE_EQ(static_cast<std::uint8_t>(StationConnectionState::address_ready),
               3U);
    REQUIRE_EQ(static_cast<std::uint8_t>(StationConnectionState::error), 4U);
}

TEST_CASE(net_021_manual_connection_disconnects_clears_and_stages_credentials) {
    StationRuntime runtime;
    runtime.state = StationConnectionState::associated;
    runtime.ssid = "old";
    runtime.password = "old-pass";
    runtime.ipv4 = "192.0.2.1";
    runtime.error_detail = "old-error";
    runtime.automatic_retry_number = 3U;
    FakeStationConnectionPort port;
    port.snapshots = {
        {StationConnectionState::associated, "reported", {}},
        {StationConnectionState::address_ready, "reported", "192.0.2.2"},
    };

    const auto result = ManualStationConnection::connect(
        runtime, port, std::string(40U, 's'), std::string(70U, 'p'));

    REQUIRE(result.success);
    REQUIRE_EQ(port.calls[0], std::string("disconnect"));
    REQUIRE_EQ(port.delays[0], 1000U);
    REQUIRE_EQ(port.calls[1], std::string("configure"));
    REQUIRE_EQ(port.calls[2], std::string("connect"));
    REQUIRE_EQ(port.ssid, std::string(31U, 's'));
    REQUIRE_EQ(port.password, std::string(63U, 'p'));
    REQUIRE(port.protected_management_optional);
    REQUIRE(runtime.error_detail.empty());
    REQUIRE_EQ(runtime.automatic_retry_number, 3U);
}

TEST_CASE(net_022_configuration_and_connection_api_failures_retain_exact_text) {
    StationRuntime runtime;
    FakeStationConnectionPort port;
    port.configure_result = {false, "ESP_ERR_WIFI_ARG"};

    auto result = ManualStationConnection::connect(runtime, port, "ssid", "pw");

    REQUIRE(!result.success);
    REQUIRE_EQ(runtime.state, StationConnectionState::error);
    REQUIRE_EQ(runtime.error_detail,
               std::string("Failed to set WiFi config: ESP_ERR_WIFI_ARG"));

    port.configure_result = {true, {}};
    port.connect_result = {false, "ESP_FAIL"};
    result = ManualStationConnection::connect(runtime, port, "ssid", "pw");

    REQUIRE(!result.success);
    REQUIRE_EQ(runtime.error_detail,
               std::string("Failed to start WiFi connection: ESP_FAIL"));
}

TEST_CASE(net_022_and_025_association_then_address_succeeds_and_saves_credentials) {
    StationRuntime runtime;
    FakeStationConnectionPort port;
    port.snapshots = {
        {StationConnectionState::associated, "actual-ap", {}},
        {StationConnectionState::address_ready, "actual-ap", "198.51.100.7"},
    };

    const auto result =
        ManualStationConnection::connect(runtime, port, "requested", "secret");

    REQUIRE(result.success);
    REQUIRE_EQ(result.ipv4, std::string("198.51.100.7"));
    REQUIRE_EQ(runtime.state, StationConnectionState::address_ready);
    REQUIRE_EQ(runtime.ssid, std::string("actual-ap"));
    REQUIRE_EQ(runtime.password, std::string("secret"));
    REQUIRE_EQ(port.persisted_ssid, std::string("actual-ap"));
    REQUIRE_EQ(port.persisted_password, std::string("secret"));
    REQUIRE_EQ(port.delays, std::vector<std::uint32_t>({100U, 100U}));
}

TEST_CASE(net_022_address_ready_before_observed_association_still_fails) {
    StationRuntime runtime;
    FakeStationConnectionPort port;
    port.snapshots = {
        {StationConnectionState::address_ready, "ap", "203.0.113.8"},
    };

    const auto result = ManualStationConnection::connect(runtime, port, "ap", {});

    REQUIRE(!result.success);
    REQUIRE_EQ(runtime.state, StationConnectionState::error);
    REQUIRE_EQ(runtime.ipv4, std::string("203.0.113.8"));
    REQUIRE_EQ(runtime.error_detail, std::string("Failed to connect to AP"));
    REQUIRE_EQ(port.delays.size(), 100U);
}

TEST_CASE(net_022_ipv4_timeout_retains_exact_error) {
    StationRuntime runtime;
    FakeStationConnectionPort port;
    port.snapshots = {
        {StationConnectionState::associated, "ap", {}},
    };

    const auto result = ManualStationConnection::connect(runtime, port, "ap", {});

    REQUIRE(!result.success);
    REQUIRE_EQ(runtime.error_detail, std::string("IP assignment timeout"));
    REQUIRE_EQ(port.delays.size(), 101U);
}

TEST_CASE(net_023_disconnect_success_clears_runtime_but_preserves_retry) {
    StationRuntime runtime;
    runtime.state = StationConnectionState::address_ready;
    runtime.ssid = "ap";
    runtime.password = "secret";
    runtime.ipv4 = "192.0.2.4";
    runtime.error_detail = "prior";
    runtime.automatic_retry_number = 3U;
    FakeStationConnectionPort port;

    const auto result = ManualStationConnection::disconnect(runtime, port);

    REQUIRE(result.success);
    REQUIRE_EQ(runtime.state, StationConnectionState::idle);
    REQUIRE(runtime.ssid.empty());
    REQUIRE(runtime.password.empty());
    REQUIRE(runtime.ipv4.empty());
    REQUIRE(runtime.error_detail.empty());
    REQUIRE_EQ(runtime.automatic_retry_number, 3U);
}

TEST_CASE(net_023_disconnect_api_failure_retains_runtime_data) {
    StationRuntime runtime;
    runtime.state = StationConnectionState::associated;
    runtime.ssid = "ap";
    runtime.password = "secret";
    runtime.ipv4 = "192.0.2.5";
    runtime.error_detail = "prior";
    FakeStationConnectionPort port;
    port.disconnect_result = {false, "ESP_FAIL"};

    const StationRuntime before = runtime;
    const auto result = ManualStationConnection::disconnect(runtime, port);

    REQUIRE(!result.success);
    REQUIRE_EQ(result.error_name, std::string("ESP_FAIL"));
    REQUIRE_EQ(runtime.state, before.state);
    REQUIRE_EQ(runtime.ssid, before.ssid);
    REQUIRE_EQ(runtime.password, before.password);
    REQUIRE_EQ(runtime.ipv4, before.ipv4);
    REQUIRE_EQ(runtime.error_detail, before.error_detail);
}

TEST_CASE(net_044_manual_connection_reports_ip_then_triggers_discovery) {
    StationRuntime runtime;
    FakeStationConnectionPort station;
    station.snapshots = {
        {StationConnectionState::associated, "ap", {}},
        {StationConnectionState::address_ready, "ap", "192.0.2.20"},
    };
    FakeWlanConnectionResponsePort responses;

    WlanConnectionCommand::connect(runtime, station, responses, "ap", "pw");

    REQUIRE_EQ(responses.sent.size(), 2U);
    REQUIRE_EQ(responses.sent[0].type, 0x83U);
    REQUIRE_EQ(text(responses.sent[0].payload),
               std::string("WiFi connected, ip: 192.0.2.20\n"));
    REQUIRE_EQ(responses.sent[1].type, 0x84U);
    REQUIRE_EQ(text(responses.sent[1].payload), std::string("ok\r\n"));
    REQUIRE_EQ(responses.delay, 500U);
    REQUIRE(responses.discovery_sent);
}

TEST_CASE(net_045_manual_connection_failure_reports_retained_detail) {
    StationRuntime runtime;
    FakeStationConnectionPort station;
    station.configure_result = {false, std::string(120U, 'e')};
    FakeWlanConnectionResponsePort responses;

    WlanConnectionCommand::connect(runtime, station, responses, "ap", "pw");

    REQUIRE_EQ(responses.sent.size(), 2U);
    REQUIRE_EQ(responses.sent[0].type, 0x83U);
    REQUIRE_EQ(text(responses.sent[0].payload),
               std::string("Error: Failed to set WiFi config: ") +
                   std::string(37U, 'e') + "\n");
    REQUIRE_EQ(responses.sent[1].type, 0x85U);
    REQUIRE_EQ(text(responses.sent[1].payload),
               std::string("Connect error.\r\n"));
    REQUIRE_EQ(responses.delay, 0U);
    REQUIRE(!responses.discovery_sent);
}

TEST_CASE(net_046_manual_disconnect_success_reports_completion) {
    StationRuntime runtime;
    runtime.state = StationConnectionState::associated;
    FakeStationConnectionPort station;
    FakeWlanConnectionResponsePort responses;

    WlanConnectionCommand::disconnect(runtime, station, responses);

    REQUIRE_EQ(responses.sent.size(), 2U);
    REQUIRE_EQ(responses.sent[0].type, 0x83U);
    REQUIRE_EQ(text(responses.sent[0].payload),
               std::string("WiFi Disconnected!\n"));
    REQUIRE_EQ(responses.sent[1].type, 0x84U);
    REQUIRE_EQ(text(responses.sent[1].payload), std::string("ok\r\n"));
}

TEST_CASE(net_046_manual_disconnect_failure_reports_api_error) {
    StationRuntime runtime;
    runtime.state = StationConnectionState::associated;
    FakeStationConnectionPort station;
    station.disconnect_result = {false, "ESP_ERR_WIFI_NOT_INIT"};
    FakeWlanConnectionResponsePort responses;

    WlanConnectionCommand::disconnect(runtime, station, responses);

    REQUIRE_EQ(responses.sent.size(), 2U);
    REQUIRE_EQ(responses.sent[0].type, 0x83U);
    REQUIRE_EQ(text(responses.sent[0].payload),
               std::string("Error: ESP_ERR_WIFI_NOT_INIT\n"));
    REQUIRE_EQ(responses.sent[1].type, 0x85U);
    REQUIRE_EQ(text(responses.sent[1].payload),
               std::string("Disconnect error.\r\n"));
}
