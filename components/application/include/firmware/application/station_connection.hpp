// Declares runtime station state and manual connection policy behind a port.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace firmware::application {

// Uses the exact numeric connection states exposed by the firmware protocol.
enum class StationConnectionState : std::uint8_t {
    idle = 0U,
    attempt_started = 1U,
    associated = 2U,
    address_ready = 3U,
    error = 4U,
};

// Retains the station data shared by commands, events, and status reporting.
struct StationRuntime {
    StationConnectionState state = StationConnectionState::idle;
    std::string ssid;
    std::string password;
    std::string ipv4;
    bool has_error = false;
    std::string error_detail;
    std::uint8_t automatic_retry_number = 0U;
};

// Reports success or the symbolic error returned by a station API operation.
struct StationApiResult {
    bool success;
    std::string error_name;
};

// Represents one asynchronous station state observed while polling.
struct StationSnapshot {
    StationConnectionState state = StationConnectionState::idle;
    std::string ssid;
    std::string ipv4;
};

// Reports the result needed by the host WLAN response layer.
struct ManualConnectionResult {
    bool success;
    std::string ipv4;
    std::string error_name;
};

// Isolates manual connection policy from ESP-IDF, time, and persistent storage.
class StationConnectionPort {
public:
    // Enables safe destruction through a substituted port implementation.
    virtual ~StationConnectionPort() = default;

    // Requests disassociation from the current station network.
    virtual StationApiResult request_disconnect() = 0;

    // Applies bounded credentials and optional protected-management frames.
    virtual StationApiResult apply_station_config(
        std::string_view ssid, std::string_view password,
        bool optional_protected_management) = 0;

    // Requests association using the currently applied station configuration.
    virtual StationApiResult request_connect() = 0;

    // Waits for a policy-defined interval.
    virtual void delay_milliseconds(std::uint32_t duration) = 0;

    // Returns the most recently observed asynchronous station state.
    virtual StationSnapshot station_snapshot() const = 0;

    // Commits both credentials after IPv4 assignment; failure is nonfatal.
    virtual StationApiResult save_credentials(
        std::string_view ssid, std::string_view password) = 0;
};

// Applies manual station connect and disconnect state transitions.
class ManualStationConnection {
public:
    // Stages credentials, waits for association and IPv4, then persists them.
    static ManualConnectionResult connect(StationRuntime& runtime,
                                          StationConnectionPort& port,
                                          std::string_view ssid,
                                          std::string_view password);

    // Clears runtime connection data only after a successful API disconnect.
    static StationApiResult disconnect(StationRuntime& runtime,
                                       StationConnectionPort& port);
};

}  // namespace firmware::application
