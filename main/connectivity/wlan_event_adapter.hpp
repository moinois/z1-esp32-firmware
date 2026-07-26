// Declares ESP-IDF Wi-Fi/IP event registration for discovery lifecycle updates.
#pragma once

#include <string>
#include <cstdint>

namespace firmware::application {
class BleProvisioning;
}

namespace firmware::target {

// Returns the latest ESP-IDF station disconnect reason for diagnostics.
std::string last_station_disconnect_detail();

// Counts station lifecycle events since the current boot.
struct WifiEventStatistics {
    std::uint32_t station_starts = 0U;
    std::uint32_t associations = 0U;
    std::uint32_t disconnections = 0U;
    std::uint32_t addresses_acquired = 0U;
    std::uint32_t addresses_lost = 0U;
    std::uint8_t last_disconnect_reason = 0U;
};

WifiEventStatistics wifi_event_statistics();

class AutomaticConnectionAdapter;

class WlanEventAdapter {
public:
    // Connects station disconnect events to the automatic retry policy.
    void set_automatic_connection(AutomaticConnectionAdapter* adapter);

    // Connects Wi-Fi station events to the active BLUFI provisioning session.
    void set_ble_provisioning(firmware::application::BleProvisioning* provisioning);

    // Returns the active provisioning policy for the event thunk.
    firmware::application::BleProvisioning* ble_provisioning() const;

    // Returns the configured automatic connection adapter for the event thunk.
    AutomaticConnectionAdapter* automatic_connection() const;

    // Registers station association and disconnection event callbacks.
    void start();

private:
    AutomaticConnectionAdapter* automatic_connection_ = nullptr;
    firmware::application::BleProvisioning* ble_provisioning_ = nullptr;
};

}  // namespace firmware::target
