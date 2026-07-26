// Verifies saved station credentials, delayed connection, and retry policy.
#include "test.hpp"

#include "firmware/application/automatic_connection.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using firmware::application::AutomaticConnectionPort;
using firmware::application::AutomaticStationConnection;
using firmware::application::StationApiResult;
using firmware::application::StationConfiguration;
using firmware::application::StationConnectionState;
using firmware::application::StationRuntime;
using firmware::application::StationRuntimeEvents;
using firmware::application::StoredString;
using firmware::application::StoredStringState;

namespace {

// Supplies persistent values and records automatic connection operations.
class FakeAutomaticConnectionPort final : public AutomaticConnectionPort {
public:
    // Returns a configured namespace/key result and records lookup order.
    StoredString read_string(std::string_view name_space,
                             std::string_view key) override {
        reads.push_back(std::string(name_space) + ":" + std::string(key));
        return key == "ssid" ? saved_ssid : saved_password;
    }

    // Records the complete station configuration and returns its API outcome.
    StationApiResult apply_station_config(
        const StationConfiguration& requested) override {
        configured = requested;
        configuration_applied = true;
        return configure_result;
    }

    // Records delayed-operation creation and returns whether it succeeded.
    bool schedule_automatic_connection(
        std::uint32_t delay_milliseconds) override {
        scheduled_delay = delay_milliseconds;
        return schedule_succeeds;
    }

    // Records one automatic association request.
    StationApiResult request_connect() override {
        ++connect_requests;
        if (runtime_to_observe != nullptr) {
            retry_observed_at_connect =
                runtime_to_observe->automatic_retry_number;
        }
        return connect_result;
    }

    // Records retry delays in exact order.
    void delay_milliseconds(std::uint32_t duration) override {
        delays.push_back(duration);
    }

    StoredString saved_ssid{StoredStringState::missing, {}, {}};
    StoredString saved_password{StoredStringState::missing, {}, {}};
    StationApiResult configure_result{true, {}};
    StationApiResult connect_result{true, {}};
    StationConfiguration configured{};
    bool configuration_applied = false;
    bool schedule_succeeds = true;
    std::uint32_t scheduled_delay = 0U;
    std::size_t connect_requests = 0U;
    const StationRuntime* runtime_to_observe = nullptr;
    std::uint8_t retry_observed_at_connect = 0U;
    std::vector<std::string> reads;
    std::vector<std::uint32_t> delays;
};

}  // namespace

TEST_CASE(net_010_to_014_saved_credentials_are_bounded_configured_and_scheduled) {
    StationRuntime runtime;
    FakeAutomaticConnectionPort port;
    port.saved_ssid = {StoredStringState::found, std::string(40U, 's'), {}};
    port.saved_password = {StoredStringState::found, std::string(70U, 'p'), {}};
    port.configure_result = {false, "ESP_FAIL"};

    const bool scheduled =
        AutomaticStationConnection::load_and_schedule(runtime, port);

    REQUIRE(scheduled);
    REQUIRE_EQ(port.reads, std::vector<std::string>({"wifi_config:ssid",
                                                     "wifi_config:password"}));
    REQUIRE_EQ(runtime.ssid, std::string(31U, 's'));
    REQUIRE_EQ(runtime.password, std::string(63U, 'p'));
    REQUIRE(port.configuration_applied);
    REQUIRE_EQ(port.configured.ssid, runtime.ssid);
    REQUIRE_EQ(port.configured.password, runtime.password);
    REQUIRE(port.configured.scan_all_channels);
    REQUIRE(port.configured.sort_by_strongest_signal);
    REQUIRE(!port.configured.use_fixed_bssid);
    REQUIRE_EQ(port.configured.channel, 0U);
    REQUIRE_EQ(port.configured.minimum_authentication_mode, 0U);
    REQUIRE(port.configured.optional_protected_management);
    REQUIRE_EQ(port.scheduled_delay, 500U);
    REQUIRE_EQ(port.connect_requests, 0U);
}

TEST_CASE(net_011_missing_ssid_or_storage_failure_disables_automatic_connection) {
    StationRuntime runtime;
    FakeAutomaticConnectionPort port;

    REQUIRE(!AutomaticStationConnection::load_and_schedule(runtime, port));
    REQUIRE(!port.configuration_applied);
    REQUIRE_EQ(port.scheduled_delay, 0U);

    port.saved_ssid = {StoredStringState::failure, {}, "ESP_FAIL"};
    REQUIRE(!AutomaticStationConnection::load_and_schedule(runtime, port));

    port.saved_ssid = {StoredStringState::found, "ap", {}};
    port.saved_password = {StoredStringState::failure, {}, "ESP_FAIL"};
    REQUIRE(!AutomaticStationConnection::load_and_schedule(runtime, port));
}

TEST_CASE(net_011_missing_password_is_staged_as_empty) {
    StationRuntime runtime;
    FakeAutomaticConnectionPort port;
    port.saved_ssid = {StoredStringState::found, "open-ap", {}};

    REQUIRE(AutomaticStationConnection::load_and_schedule(runtime, port));
    REQUIRE_EQ(runtime.ssid, std::string("open-ap"));
    REQUIRE(runtime.password.empty());
}

TEST_CASE(net_014_schedule_failure_leaves_credentials_without_connecting) {
    StationRuntime runtime;
    FakeAutomaticConnectionPort port;
    port.saved_ssid = {StoredStringState::found, "ap", {}};
    port.schedule_succeeds = false;

    REQUIRE(!AutomaticStationConnection::load_and_schedule(runtime, port));
    REQUIRE_EQ(runtime.ssid, std::string("ap"));
    REQUIRE_EQ(port.connect_requests, 0U);
    REQUIRE_EQ(runtime.automatic_retry_number, 0U);
}

TEST_CASE(net_014_delayed_automatic_connection_seeds_retry_before_request) {
    StationRuntime runtime;
    FakeAutomaticConnectionPort port;
    port.runtime_to_observe = &runtime;

    const StationApiResult result =
        AutomaticStationConnection::begin_scheduled(runtime, port);

    REQUIRE(result.success);
    REQUIRE_EQ(runtime.automatic_retry_number, 1U);
    REQUIRE_EQ(port.connect_requests, 1U);
    REQUIRE_EQ(port.retry_observed_at_connect, 1U);
}

TEST_CASE(net_015_disconnect_retries_one_through_four_then_stops) {
    StationRuntime runtime;
    FakeAutomaticConnectionPort port;
    runtime.automatic_retry_number = 1U;

    for (std::uint8_t expected = 2U; expected <= 5U; ++expected) {
        REQUIRE(AutomaticStationConnection::handle_disconnect(runtime, port));
        REQUIRE_EQ(runtime.automatic_retry_number, expected);
    }
    REQUIRE(!AutomaticStationConnection::handle_disconnect(runtime, port));

    REQUIRE_EQ(runtime.automatic_retry_number, 0U);
    REQUIRE_EQ(port.connect_requests, 4U);
    REQUIRE_EQ(port.delays,
               std::vector<std::uint32_t>({1000U, 200U, 1000U, 200U,
                                           1000U, 200U, 1000U, 200U}));
}

TEST_CASE(net_015_and_025_station_events_update_runtime_and_reset_retry) {
    StationRuntime runtime;
    runtime.ssid = "requested";
    runtime.password = "secret";
    runtime.automatic_retry_number = 4U;

    StationRuntimeEvents::associated(runtime, std::string(40U, 'a'));

    REQUIRE_EQ(runtime.state, StationConnectionState::associated);
    REQUIRE_EQ(runtime.ssid, std::string(31U, 'a'));
    REQUIRE_EQ(runtime.password, std::string("secret"));
    REQUIRE_EQ(runtime.automatic_retry_number, 4U);

    StationRuntimeEvents::address_ready(runtime, "192.0.2.30");

    REQUIRE_EQ(runtime.state, StationConnectionState::address_ready);
    REQUIRE_EQ(runtime.ipv4, std::string("192.0.2.30"));
    REQUIRE_EQ(runtime.automatic_retry_number, 0U);
}

TEST_CASE(net_024_current_rssi_is_observed_value_or_zero) {
    REQUIRE_EQ(StationRuntimeEvents::current_rssi(true, -71), -71);
    REQUIRE_EQ(StationRuntimeEvents::current_rssi(false, -71), 0);
}
