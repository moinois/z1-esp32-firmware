// Implements ESP-IDF station configuration, association, and IPv4 polling.
#include "tcp_wlan_station_adapter.hpp"

#include "nvs_key_value_adapter.hpp"

#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstring>
#include <lwip/inet.h>
#include <lwip/sockets.h>

namespace firmware::target {
namespace {

firmware::application::StationApiResult api_result(esp_err_t result,
                                                    const char* operation) {
    return result == ESP_OK
        ? firmware::application::StationApiResult{true, {}}
        : firmware::application::StationApiResult{false, operation};
}

}  // namespace

firmware::application::StationApiResult
TcpWlanStationAdapter::request_disconnect() {
    return api_result(esp_wifi_disconnect(), "disconnect");
}

firmware::application::StationApiResult
TcpWlanStationAdapter::apply_station_config(
    const firmware::application::StationConfiguration& configuration) {
    wifi_config_t wifi_config{};
    const std::size_t ssid_size = configuration.ssid.size() < sizeof(wifi_config.sta.ssid)
        ? configuration.ssid.size() : sizeof(wifi_config.sta.ssid);
    const std::size_t password_size = configuration.password.size() < sizeof(wifi_config.sta.password)
        ? configuration.password.size() : sizeof(wifi_config.sta.password);
    std::memcpy(wifi_config.sta.ssid, configuration.ssid.data(), ssid_size);
    std::memcpy(wifi_config.sta.password, configuration.password.data(), password_size);
    wifi_config.sta.scan_method = WIFI_FAST_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    wifi_config.sta.threshold.authmode = static_cast<wifi_auth_mode_t>(
        configuration.minimum_authentication_mode);
    const esp_err_t mode_result = esp_wifi_set_mode(WIFI_MODE_STA);
    if (mode_result != ESP_OK) {
        return api_result(mode_result, "set_mode");
    }
    return api_result(esp_wifi_set_config(WIFI_IF_STA, &wifi_config),
                      "set_config");
}

firmware::application::StationApiResult
TcpWlanStationAdapter::request_connect() {
    return api_result(esp_wifi_connect(), "connect");
}

void TcpWlanStationAdapter::delay_milliseconds(std::uint32_t duration) {
    vTaskDelay(pdMS_TO_TICKS(duration));
}

firmware::application::StationSnapshot
TcpWlanStationAdapter::station_snapshot() const {
    wifi_ap_record_t access_point{};
    if (esp_wifi_sta_get_ap_info(&access_point) != ESP_OK) {
        return {};
    }
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == nullptr) {
        return {firmware::application::StationConnectionState::associated,
                std::string(reinterpret_cast<const char*>(access_point.ssid)), {}};
    }
    esp_netif_ip_info_t ip_info{};
    if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK || ip_info.ip.addr == 0U) {
        return {firmware::application::StationConnectionState::associated,
                std::string(reinterpret_cast<const char*>(access_point.ssid)), {}};
    }
    char address[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &ip_info.ip.addr, address, sizeof(address));
    return {firmware::application::StationConnectionState::address_ready,
            std::string(reinterpret_cast<const char*>(access_point.ssid)), address};
}

firmware::application::StationApiResult
TcpWlanStationAdapter::save_credentials(std::string_view ssid,
                                        std::string_view password) {
    NvsKeyValueAdapter nvs;
    if (!nvs.write_string("wifi", "ssid", ssid)) {
        return {false, "save_ssid"};
    }
    if (!nvs.write_string("wifi", "password", password)) {
        return {false, "save_password"};
    }
    return {true, {}};
}

std::string TcpWlanStationAdapter::current_netmask() const {
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == nullptr) {
        return {};
    }
    esp_netif_ip_info_t ip_info{};
    if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) {
        return {};
    }
    char address[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &ip_info.netmask.addr, address, sizeof(address));
    return std::string(address);
}

}  // namespace firmware::target
