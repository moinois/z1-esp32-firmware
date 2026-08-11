/** @file @brief Declares the ESP-IDF implementation of the application connectivity port. */
#pragma once

#include "application/connectivity/connectivity_startup.hpp"

namespace firmware::target {

/// Translates the portable startup policy into ESP-IDF Wi-Fi/netif calls.
class ConnectivityStartupAdapter final
    : public firmware::application::ConnectivityStartupPort {
public:
    /// Reads and normalizes the persistent `softap` startup settings.
    firmware::application::AccessPointStartupSettings load_settings() const;
    bool enter_station_only_mode() override;
    firmware::application::StartupScanOutcome scan_channels(
        const firmware::application::StartupScanConfig& config) override;
    bool start_access_point_and_station(
        const firmware::application::ConnectivityStartupConfig& config) override;
    bool finish_station_only_mode() override;
};

}  // namespace firmware::target
