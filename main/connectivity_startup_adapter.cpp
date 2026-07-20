// Implements the station scan and concurrent AP/STA setup required at boot.
#include "connectivity_startup_adapter.hpp"

#include "esp_netif.h"
#include "esp_wifi.h"

#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>

namespace firmware::target {
namespace {

using firmware::application::StartupScanOutcome;
using firmware::application::StartupScanState;

StartupScanOutcome scan_failure(StartupScanState state) {
    return StartupScanOutcome{state, {}};
}

}  // namespace

bool ConnectivityStartupAdapter::enter_station_only_mode() {
    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK) {
        return false;
    }
    return esp_wifi_start() == ESP_OK;
}

StartupScanOutcome ConnectivityStartupAdapter::scan_channels(
    const firmware::application::StartupScanConfig& policy) {
    wifi_scan_config_t scan{};
    scan.scan_type = policy.active ? WIFI_SCAN_TYPE_ACTIVE : WIFI_SCAN_TYPE_PASSIVE;
    scan.show_hidden = policy.include_hidden;
    scan.scan_time.active.min = policy.minimum_active_dwell_milliseconds;
    scan.scan_time.active.max = policy.maximum_active_dwell_milliseconds;
    if (esp_wifi_scan_start(&scan, policy.blocking) != ESP_OK) {
        return scan_failure(StartupScanState::start_failure);
    }

    std::uint16_t count = 0U;
    if (esp_wifi_scan_get_ap_num(&count) != ESP_OK) {
        return scan_failure(StartupScanState::count_failure);
    }
    std::vector<wifi_ap_record_t> records(count);
    if (count > 0U && esp_wifi_scan_get_ap_records(&count, records.data()) != ESP_OK) {
        return scan_failure(StartupScanState::records_failure);
    }

    std::vector<std::uint8_t> channels;
    channels.reserve(count);
    for (std::uint16_t index = 0U; index < count; ++index) {
        const std::uint8_t channel = records[index].primary;
        if (channel != 0U && std::find(channels.begin(), channels.end(), channel)
                                  == channels.end()) {
            channels.push_back(channel);
        }
    }
    return StartupScanOutcome{StartupScanState::success, std::move(channels)};
}

bool ConnectivityStartupAdapter::start_access_point_and_station(
    const firmware::application::ConnectivityStartupConfig& policy) {
    if (esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK) {
        return false;
    }

    wifi_config_t access_point{};
    std::memcpy(access_point.ap.ssid, policy.access_point_ssid.data(),
                std::min(policy.access_point_ssid.size(), sizeof(access_point.ap.ssid)));
    access_point.ap.ssid_len = static_cast<std::uint8_t>(
        std::min(policy.access_point_ssid.size(), sizeof(access_point.ap.ssid)));
    access_point.ap.channel = policy.channel;
    access_point.ap.max_connection = policy.maximum_clients;
    access_point.ap.authmode = policy.open_authentication
                                   ? WIFI_AUTH_OPEN
                                   : WIFI_AUTH_WPA2_PSK;
    if (esp_wifi_set_config(WIFI_IF_AP, &access_point) != ESP_OK) {
        return false;
    }

    wifi_config_t station{};
    station.sta.scan_method = WIFI_FAST_SCAN;
    station.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    if (esp_wifi_set_config(WIFI_IF_STA, &station) != ESP_OK) {
        return false;
    }
    // Wi-Fi was started for the station-only scan; changing mode keeps it running.
    return true;
}

}  // namespace firmware::target
