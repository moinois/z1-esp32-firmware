/** @file @brief Declares saved-credential startup and automatic station retry policy. */
#pragma once

#include "application/connectivity/station_connection.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace firmware::application {

/// Distinguishes a stored string from a missing key and a storage failure.
enum class StoredStringState {
    found,
    missing,
    failure,
};

/// Holds one persistent string lookup result.
struct StoredString {
    StoredStringState state;
    std::string value;
    std::string error_name;
};

/// Isolates automatic connection policy from storage, Wi-Fi, tasks, and time.
class AutomaticConnectionPort {
public:
    /// Enables safe destruction through a substituted adapter.
    virtual ~AutomaticConnectionPort() = default;

    /// Reads one string from a persistent namespace and key.
    virtual StoredString read_string(std::string_view name_space,
                                     std::string_view key) = 0;

    /// Applies loaded credentials and the saved-network selection policy.
    virtual StationApiResult apply_station_config(
        const StationConfiguration& configuration) = 0;

    /// Creates the delayed automatic connection operation.
    virtual bool schedule_automatic_connection(
        std::uint32_t delay_milliseconds) = 0;

    /// Requests association with the staged station configuration.
    virtual StationApiResult request_connect() = 0;

    /// Waits for an automatic retry policy interval.
    virtual void delay_milliseconds(std::uint32_t duration) = 0;
};

/// Loads saved credentials and owns delayed automatic reconnect counters.
class AutomaticStationConnection {
public:
    /// Stages a usable saved network and schedules its delayed operation.
    static bool load_and_schedule(StationRuntime& runtime,
                                  AutomaticConnectionPort& port);

    /// Seeds retry number one immediately before requesting association.
    static StationApiResult begin_scheduled(StationRuntime& runtime,
                                            AutomaticConnectionPort& port);

    /// Applies one disconnect event and reports whether reconnect was requested.
    static bool handle_disconnect(StationRuntime& runtime,
                                  AutomaticConnectionPort& port);
};

}  // namespace firmware::application
