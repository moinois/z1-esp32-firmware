/** @file @brief Portable Wi-Fi diagnostic snapshot and JSON representation. */
#pragma once

#include <cstdint>
#include <string>

namespace firmware::core {

/** Point-in-time station state plus monotonic lifecycle counters. */
struct WifiStatistics {
    /// Whether a usable station association currently exists.
    bool connected = false;
    /// Current signal strength in dBm, or zero when unavailable.
    std::int32_t rssi_dbm = 0;
    /// Current RF channel, or zero when unavailable.
    std::uint8_t channel = 0U;
    /// Stable human-readable authentication mode.
    std::string authentication = "unavailable";
    /// Assigned station IPv4 address or the zero address when disconnected.
    std::string ipv4_address = "0.0.0.0";
    /// Number of station-driver start events since boot.
    std::uint32_t station_starts = 0U;
    /// Number of association events since boot.
    std::uint32_t associations = 0U;
    /// Number of disassociation events since boot.
    std::uint32_t disconnections = 0U;
    /// Number of IPv4 acquisition events since boot.
    std::uint32_t addresses_acquired = 0U;
    /// Number of IPv4 loss events since boot.
    std::uint32_t addresses_lost = 0U;
    /// Target SDK reason code for the latest disconnect.
    std::uint8_t last_disconnect_reason = 0U;
    /// Target reset reason associated with the running boot.
    std::uint32_t reset_reason = 0U;
    /// Bounded recent-event trace for diagnosing intermittent RF behavior.
    std::string recent_events;
};

/** Formats a stable JSON object, escaping retained event text safely. */
std::string format_wifi_statistics_json(const WifiStatistics& statistics);

}  // namespace firmware::core
