// Implements bounded manual station connection and disconnection state policy.
#include "firmware/application/station_connection.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace firmware::application {
namespace {

constexpr std::size_t maximum_ssid_size = 31U;
constexpr std::size_t maximum_password_size = 63U;
constexpr std::size_t maximum_error_detail_size = 64U;
constexpr std::uint32_t prior_connection_settle_milliseconds = 1000U;
constexpr std::uint32_t poll_interval_milliseconds = 100U;
constexpr std::uint32_t connection_phase_timeout_milliseconds = 10000U;
constexpr bool optional_protected_management = true;

// Copies at most the field capacity without exposing fixed buffers to policy.
std::string bounded(std::string_view value, std::size_t maximum_size) {
    return std::string(value.substr(0U, maximum_size));
}

// Clears connection fields while deliberately preserving automatic retry state.
void clear_connection_data(StationRuntime& runtime) {
    runtime.state = StationConnectionState::idle;
    runtime.ssid.clear();
    runtime.password.clear();
    runtime.ipv4.clear();
    runtime.has_error = false;
    runtime.error_detail.clear();
}

// Retains a bounded failure and maps runtime state to the specified error value.
ManualConnectionResult fail(StationRuntime& runtime, std::string detail,
                            std::string error_name = {}) {
    runtime.state = StationConnectionState::error;
    runtime.has_error = true;
    runtime.error_detail = bounded(detail, maximum_error_detail_size);
    return {false, runtime.ipv4, std::move(error_name)};
}

// Applies event-derived identity and address fields to retained runtime state.
void retain_snapshot(StationRuntime& runtime, const StationSnapshot& snapshot) {
    if (snapshot.state == StationConnectionState::associated) {
        runtime.state = StationConnectionState::associated;
        runtime.ssid = bounded(snapshot.ssid, maximum_ssid_size);
    } else if (snapshot.state == StationConnectionState::address_ready) {
        runtime.state = StationConnectionState::address_ready;
        runtime.ssid = bounded(snapshot.ssid, maximum_ssid_size);
        runtime.ipv4 = snapshot.ipv4;
    }
}

}  // namespace

ManualConnectionResult ManualStationConnection::connect(
    StationRuntime& runtime, StationConnectionPort& port, std::string_view ssid,
    std::string_view password) {
    if (runtime.state != StationConnectionState::idle) {
        static_cast<void>(port.request_disconnect());
        port.delay_milliseconds(prior_connection_settle_milliseconds);
    }

    clear_connection_data(runtime);
    runtime.ssid = bounded(ssid, maximum_ssid_size);
    runtime.password = bounded(password, maximum_password_size);

    const StationApiResult configured = port.apply_station_config(
        runtime.ssid, runtime.password, optional_protected_management);
    if (!configured.success) {
        return fail(runtime, "Failed to set WiFi config: " +
                                 configured.error_name,
                    configured.error_name);
    }

    runtime.state = StationConnectionState::attempt_started;
    const StationApiResult started = port.request_connect();
    if (!started.success) {
        return fail(runtime, "Failed to start WiFi connection: " +
                                 started.error_name,
                    started.error_name);
    }

    bool associated = false;
    for (std::uint32_t elapsed = 0U;
         elapsed < connection_phase_timeout_milliseconds;
         elapsed += poll_interval_milliseconds) {
        port.delay_milliseconds(poll_interval_milliseconds);
        const StationSnapshot snapshot = port.station_snapshot();
        retain_snapshot(runtime, snapshot);
        if (snapshot.state == StationConnectionState::associated) {
            associated = true;
            break;
        }
    }
    if (!associated) {
        return fail(runtime, "Failed to connect to AP");
    }

    for (std::uint32_t elapsed = 0U;
         elapsed < connection_phase_timeout_milliseconds;
         elapsed += poll_interval_milliseconds) {
        port.delay_milliseconds(poll_interval_milliseconds);
        const StationSnapshot snapshot = port.station_snapshot();
        retain_snapshot(runtime, snapshot);
        if (snapshot.state != StationConnectionState::address_ready) {
            continue;
        }
        runtime.has_error = false;
        runtime.error_detail.clear();
        static_cast<void>(port.save_credentials(runtime.ssid, runtime.password));
        return {true, runtime.ipv4, {}};
    }
    return fail(runtime, "IP assignment timeout");
}

StationApiResult ManualStationConnection::disconnect(
    StationRuntime& runtime, StationConnectionPort& port) {
    const StationApiResult result = port.request_disconnect();
    if (result.success) {
        clear_connection_data(runtime);
    }
    return result;
}

}  // namespace firmware::application
