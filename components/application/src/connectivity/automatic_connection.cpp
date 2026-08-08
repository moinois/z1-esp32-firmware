/** @file @brief Implements persistent station credential loading and automatic retry policy. */
#include "firmware/application/automatic_connection.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace firmware::application {
namespace {

constexpr std::string_view credential_namespace = "wifi_config";
constexpr std::string_view ssid_key = "ssid";
constexpr std::string_view password_key = "password";
constexpr std::size_t maximum_ssid_size = 31U;
constexpr std::size_t maximum_password_size = 63U;
constexpr std::uint32_t automatic_connection_delay_milliseconds = 500U;
constexpr std::uint8_t first_automatic_retry = 1U;
constexpr std::uint8_t final_reconnecting_retry = 4U;
constexpr std::uint32_t retry_disconnect_delay_milliseconds = 1000U;
constexpr std::uint32_t retry_connect_delay_milliseconds = 200U;

// Copies one stored credential without exceeding its runtime capacity.
std::string bounded(std::string_view value, std::size_t maximum_size) {
    return std::string(value.substr(0U, maximum_size));
}

}  // namespace

bool AutomaticStationConnection::load_and_schedule(
    StationRuntime& runtime, AutomaticConnectionPort& port) {
    const StoredString stored_ssid =
        port.read_string(credential_namespace, ssid_key);
    if (stored_ssid.state != StoredStringState::found ||
        stored_ssid.value.empty()) {
        return false;
    }

    const StoredString stored_password =
        port.read_string(credential_namespace, password_key);
    if (stored_password.state == StoredStringState::failure) {
        return false;
    }

    runtime.ssid = bounded(stored_ssid.value, maximum_ssid_size);
    runtime.password = stored_password.state == StoredStringState::found
                           ? bounded(stored_password.value,
                                     maximum_password_size)
                           : std::string{};
    StationConfiguration configuration;
    configuration.ssid = runtime.ssid;
    configuration.password = runtime.password;
    static_cast<void>(port.apply_station_config(configuration));
    return port.schedule_automatic_connection(
        automatic_connection_delay_milliseconds);
}

StationApiResult AutomaticStationConnection::begin_scheduled(
    StationRuntime& runtime, AutomaticConnectionPort& port) {
    runtime.automatic_retry_number = first_automatic_retry;
    runtime.state = StationConnectionState::attempt_started;
    return port.request_connect();
}

bool AutomaticStationConnection::handle_disconnect(
    StationRuntime& runtime, AutomaticConnectionPort& port) {
    if (runtime.automatic_retry_number < first_automatic_retry ||
        runtime.automatic_retry_number > final_reconnecting_retry) {
        if (runtime.automatic_retry_number > final_reconnecting_retry) {
            runtime.automatic_retry_number = 0U;
        }
        return false;
    }

    port.delay_milliseconds(retry_disconnect_delay_milliseconds);
    ++runtime.automatic_retry_number;
    port.delay_milliseconds(retry_connect_delay_milliseconds);
    runtime.state = StationConnectionState::attempt_started;
    static_cast<void>(port.request_connect());
    return true;
}

}  // namespace firmware::application
