// Declares the portable Wi-Fi diagnostic snapshot and JSON representation.
#pragma once

#include <cstdint>
#include <string>

namespace firmware::core {

struct WifiStatistics {
    bool connected = false;
    std::int32_t rssi_dbm = 0;
    std::uint8_t channel = 0U;
    std::string authentication = "unavailable";
    std::string ipv4_address = "0.0.0.0";
    std::uint32_t station_starts = 0U;
    std::uint32_t associations = 0U;
    std::uint32_t disconnections = 0U;
    std::uint32_t addresses_acquired = 0U;
    std::uint32_t addresses_lost = 0U;
    std::uint8_t last_disconnect_reason = 0U;
    std::string recent_events;
};

// Formats a stable JSON object, escaping the retained event log safely.
std::string format_wifi_statistics_json(const WifiStatistics& statistics);

}  // namespace firmware::core
