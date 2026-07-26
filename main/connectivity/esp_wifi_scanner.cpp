// Implements transport-independent ESP-IDF Wi-Fi scanning and conversion.
#include "esp_wifi_scanner.hpp"

#include "esp_wifi.h"

#include <utility>
#include <vector>

namespace firmware::target {

void EspWifiScanner::stop_scan() const {
    static_cast<void>(esp_wifi_scan_stop());
}

firmware::application::WifiScanOutcome EspWifiScanner::scan(
    const firmware::application::WifiScanConfig& config) const {
    wifi_scan_config_t scan_config{};
    scan_config.scan_type = config.active ? WIFI_SCAN_TYPE_ACTIVE
                                          : WIFI_SCAN_TYPE_PASSIVE;
    scan_config.show_hidden = config.include_hidden;
    scan_config.scan_time.active.min = config.active_dwell_milliseconds;
    scan_config.scan_time.active.max = config.active_dwell_milliseconds;
    scan_config.scan_time.passive = config.passive_dwell_milliseconds;
    if (esp_wifi_scan_start(&scan_config, true) != ESP_OK) {
        return {false, "scan_start", {}};
    }
    std::uint16_t count = 0U;
    if (esp_wifi_scan_get_ap_num(&count) != ESP_OK) {
        return {false, "scan_count", {}};
    }
    count = static_cast<std::uint16_t>(
        count > config.maximum_observations ? config.maximum_observations : count);
    std::vector<wifi_ap_record_t> records(count);
    if (count > 0U && esp_wifi_scan_get_ap_records(&count, records.data()) != ESP_OK) {
        return {false, "scan_records", {}};
    }
    std::vector<firmware::core::WifiObservation> observations;
    observations.reserve(count);
    for (std::uint16_t index = 0U; index < count; ++index) {
        const wifi_ap_record_t& record = records[index];
        std::size_t length = 0U;
        while (length < sizeof(record.ssid) && record.ssid[length] != 0U) ++length;
        observations.push_back({
            firmware::core::ByteVector(record.ssid, record.ssid + length),
            record.rssi, static_cast<std::uint8_t>(record.authmode)});
    }
    return {true, {}, std::move(observations)};
}

std::string EspWifiScanner::connected_ssid() const {
    wifi_ap_record_t record{};
    if (esp_wifi_sta_get_ap_info(&record) != ESP_OK) return {};
    std::size_t length = 0U;
    while (length < sizeof(record.ssid) && record.ssid[length] != 0U) ++length;
    return std::string(reinterpret_cast<const char*>(record.ssid), length);
}

}  // namespace firmware::target
