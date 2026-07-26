// Declares shared ESP-IDF Wi-Fi scan operations for every response transport.
#pragma once

#include "firmware/application/wlan_command.hpp"

namespace firmware::target {

class EspWifiScanner {
public:
    void stop_scan() const;
    firmware::application::WifiScanOutcome scan(
        const firmware::application::WifiScanConfig& config) const;
    std::string connected_ssid() const;
};

}  // namespace firmware::target
