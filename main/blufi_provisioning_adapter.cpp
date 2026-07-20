// Implements ESP-IDF provisioning actions behind the portable BLUFI policy.
#include "blufi_provisioning_adapter.hpp"

#include "esp_blufi.h"
#include "esp_blufi_api.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "blufi_lifecycle_adapter.hpp"

#include <algorithm>
#include <cstring>
#include <string>

namespace firmware::target {
namespace {

constexpr char tag[] = "BLUFI_PORT";

firmware::application::StationApiResult api_result(esp_err_t result,
                                                    const char* operation) {
    return result == ESP_OK
        ? firmware::application::StationApiResult{true, {}}
        : firmware::application::StationApiResult{false, operation};
}

}  // namespace

bool BlufiProvisioningAdapter::initialize(
    const firmware::application::BleLifecycleConfig&) {
    // The lifecycle adapter owns profile registration; this port is ready after it.
    return true;
}

bool BlufiProvisioningAdapter::start_advertising(std::string_view device_name) {
    static_cast<void>(device_name);
    if (!restart_blufi_advertising()) {
        return false;
    }
    return true;
}

void BlufiProvisioningAdapter::stop_advertising() {
    esp_blufi_adv_stop();
}

void BlufiProvisioningAdapter::create_security_context() {}

void BlufiProvisioningAdapter::destroy_security_context() {}

firmware::application::StationApiResult BlufiProvisioningAdapter::apply_station_config(
    const firmware::application::StationConfiguration& configuration) {
    wifi_config_t wifi_config{};
    const std::size_t ssid_size = std::min(configuration.ssid.size(),
                                           sizeof(wifi_config.sta.ssid));
    const std::size_t password_size = std::min(configuration.password.size(),
                                               sizeof(wifi_config.sta.password));
    std::memcpy(wifi_config.sta.ssid, configuration.ssid.data(), ssid_size);
    std::memcpy(wifi_config.sta.password, configuration.password.data(), password_size);
    wifi_config.sta.scan_method = WIFI_FAST_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    if (esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK) {
        return {false, "set_mode"};
    }
    return api_result(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), "set_config");
}

firmware::application::StationApiResult
BlufiProvisioningAdapter::request_station_disconnect() {
    return api_result(esp_wifi_disconnect(), "disconnect");
}

firmware::application::StationApiResult
BlufiProvisioningAdapter::request_station_connect() {
    return api_result(esp_wifi_connect(), "connect");
}

std::uint8_t BlufiProvisioningAdapter::current_wifi_mode() const {
    wifi_mode_t mode = WIFI_MODE_NULL;
    return esp_wifi_get_mode(&mode) == ESP_OK ? static_cast<std::uint8_t>(mode) : 0U;
}

void BlufiProvisioningAdapter::stop_scan() {
    static_cast<void>(esp_wifi_scan_stop());
}

void BlufiProvisioningAdapter::delay_milliseconds(std::uint32_t duration) {
    vTaskDelay(pdMS_TO_TICKS(duration));
}

firmware::application::BleWifiScanOutcome BlufiProvisioningAdapter::scan(
    const firmware::application::WifiScanConfig& policy) {
    wifi_scan_config_t scan_config{};
    scan_config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    scan_config.show_hidden = true;
    scan_config.scan_time.active.min = policy.active_dwell_milliseconds;
    scan_config.scan_time.active.max = policy.active_dwell_milliseconds;
    if (esp_wifi_scan_start(&scan_config, true) != ESP_OK) {
        return {false, true, {}};
    }
    std::uint16_t count = 0U;
    if (esp_wifi_scan_get_ap_num(&count) != ESP_OK) {
        return {false, true, {}};
    }
    std::vector<wifi_ap_record_t> records(count);
    if (count > 0U && esp_wifi_scan_get_ap_records(&count, records.data()) != ESP_OK) {
        return {false, true, {}};
    }
    std::vector<firmware::core::WifiObservation> observations;
    observations.reserve(count);
    for (std::uint16_t index = 0U; index < count; ++index) {
        observations.push_back({
            firmware::core::ByteVector(
                records[index].ssid,
                records[index].ssid + strnlen(
                    reinterpret_cast<const char*>(records[index].ssid),
                    sizeof(records[index].ssid))),
            records[index].rssi,
            static_cast<std::uint8_t>(records[index].authmode)});
    }
    return {true, true, std::move(observations)};
}

void BlufiProvisioningAdapter::report_error(std::uint8_t error) {
    static_cast<void>(esp_blufi_send_error_info(
        static_cast<esp_blufi_error_state_t>(error)));
}

void BlufiProvisioningAdapter::send_wifi_status(
    const firmware::application::BleWifiStatusReport& report) {
    esp_blufi_extra_info_t info{};
    info.sta_bssid_set = true;
    std::copy(report.bssid.begin(), report.bssid.end(), info.sta_bssid);
    info.sta_ssid = reinterpret_cast<uint8_t*>(const_cast<char*>(report.ssid.data()));
    info.sta_ssid_len = static_cast<int>(report.ssid.size());
    static_cast<void>(esp_blufi_send_wifi_conn_report(
        static_cast<wifi_mode_t>(report.wifi_mode),
        static_cast<esp_blufi_sta_conn_state_t>(report.station_state), 0U, &info));
}

void BlufiProvisioningAdapter::send_wifi_list(
    const std::vector<firmware::application::BleWifiListEntry>& entries) {
    std::vector<esp_blufi_ap_record_t> records(entries.size());
    for (std::size_t index = 0U; index < entries.size(); ++index) {
        const std::size_t size = std::min(entries[index].ssid.size(),
                                          sizeof(records[index].ssid) - 1U);
        std::memcpy(records[index].ssid, entries[index].ssid.data(), size);
        records[index].ssid[size] = 0U;
        records[index].rssi = static_cast<std::int8_t>(entries[index].rssi);
    }
    static_cast<void>(esp_blufi_send_wifi_list(
        static_cast<std::uint16_t>(records.size()), records.data()));
}

void BlufiProvisioningAdapter::log_custom_data(firmware::core::BytesView data) {
    ESP_LOG_BUFFER_HEXDUMP(tag, data.data(), data.size(), ESP_LOG_INFO);
}

}  // namespace firmware::target
