/** @file @brief Implements NVS, configuration, radio, and APQ target access. */
#include "access_point_command_adapter.hpp"

#include "configuration_file_store.hpp"
#include "nvs_key_value_adapter.hpp"

#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_err.h"
#include "esp_log.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

namespace firmware::target {
namespace {

constexpr std::string_view softap_namespace = "softap";
constexpr std::string_view channel_key = "ch";
constexpr std::string_view password_key = "pass";
constexpr std::string_view enabled_key = "en";
constexpr std::string_view machine_name_key = "wifi.machine_name";
constexpr std::size_t maximum_query_ssid_size = 32U;
constexpr std::size_t maximum_query_password_size = 64U;

std::mutex command_mutex;
firmware::application::AccessPointCommandState command_state;

std::string bounded_c_string(const std::uint8_t* value, std::size_t capacity,
                             std::size_t limit) {
    const auto* bytes = reinterpret_cast<const char*>(value);
    return std::string(bytes, std::min(::strnlen(bytes, capacity), limit));
}

std::optional<std::string> interface_ipv4(const char* key, unsigned field,
                                          esp_err_t& error) {
    esp_netif_t* interface = esp_netif_get_handle_from_ifkey(key);
    esp_netif_ip_info_t info{};
    if (interface == nullptr) {
        error = ESP_ERR_NOT_FOUND;
        return std::nullopt;
    }
    error = esp_netif_get_ip_info(interface, &info);
    if (error != ESP_OK) {
        return std::nullopt;
    }
    const esp_ip4_addr_t* address = field == 0U ? &info.ip : field == 1U ? &info.netmask
                                                                      : &info.gw;
    std::array<char, 16U> output{};
    if (esp_ip4addr_ntoa(address, output.data(), output.size()) == nullptr) {
        error = ESP_FAIL;
        return std::nullopt;
    }
    return std::string(output.data());
}

class TargetAccessPointPort final
    : public firmware::application::AccessPointCommandPort {
public:
    bool persist_channel(std::optional<std::uint8_t> channel) override {
        return NvsKeyValueAdapter{}.write_string(
            softap_namespace, channel_key,
            channel.has_value() ? std::to_string(*channel) : std::string{});
    }

    bool persist_password(std::string_view password) override {
        return NvsKeyValueAdapter{}.write_string(softap_namespace, password_key,
                                                 password);
    }

    bool persist_enabled(bool enabled) override {
        return NvsKeyValueAdapter{}.write_u8(softap_namespace, enabled_key,
                                             enabled ? 1U : 0U);
    }

    bool persist_machine_name(std::string_view name) override {
        return ConfigurationFileStore{}.set_raw_key(machine_name_key, name);
    }

    bool enable_access_point(
        const firmware::application::AccessPointRadioConfig& config) override {
        if (esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK) return false;
        wifi_config_t wifi{};
        const std::size_t ssid_size =
            std::min(config.ssid.size(), sizeof(wifi.ap.ssid));
        const std::size_t password_size =
            std::min(config.password.size(), sizeof(wifi.ap.password));
        std::memcpy(wifi.ap.ssid, config.ssid.data(), ssid_size);
        std::memcpy(wifi.ap.password, config.password.data(), password_size);
        wifi.ap.ssid_len = static_cast<std::uint8_t>(ssid_size);
        wifi.ap.channel = config.channel;
        wifi.ap.max_connection = 4U;
        wifi.ap.authmode = config.password.empty() ? WIFI_AUTH_OPEN
                                                   : WIFI_AUTH_WPA2_PSK;
        return esp_wifi_set_config(WIFI_IF_AP, &wifi) == ESP_OK;
    }

    bool disable_access_point() override {
        return esp_wifi_set_mode(WIFI_MODE_STA) == ESP_OK;
    }

    std::optional<std::string> station_parameter(std::uint8_t parameter) override {
        wifi_config_t config{};
        if (parameter <= 2U) {
            last_query_error_ = esp_wifi_get_config(WIFI_IF_STA, &config);
            if (last_query_error_ != ESP_OK) return std::nullopt;
        }
        if (parameter == 0U) {
            return bounded_c_string(config.sta.ssid, sizeof(config.sta.ssid),
                                    maximum_query_ssid_size);
        }
        if (parameter == 1U) {
            return bounded_c_string(config.sta.password, sizeof(config.sta.password),
                                    maximum_query_password_size);
        }
        if (parameter == 2U) {
            wifi_ap_record_t record{};
            return std::to_string(esp_wifi_sta_get_ap_info(&record) == ESP_OK
                                      ? record.rssi
                                      : 0);
        }
        if (parameter == 3U) {
            esp_netif_t* interface = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
            const char* hostname = nullptr;
            last_query_error_ = interface == nullptr
                                    ? ESP_ERR_NOT_FOUND
                                    : esp_netif_get_hostname(interface, &hostname);
            auto hostname_value = interface != nullptr &&
                           last_query_error_ == ESP_OK &&
                           hostname != nullptr
                       ? std::optional<std::string>(hostname)
                       : std::nullopt;
            if (!hostname_value.has_value() && last_query_error_ == ESP_OK) {
                last_query_error_ = ESP_ERR_INVALID_RESPONSE;
            }
            if (hostname_value.has_value() && hostname_value->size() > 95U) {
                hostname_value->resize(95U);
            }
            return hostname_value;
        }
        if (parameter == 4U) {
            std::array<std::uint8_t, 6U> mac{};
            last_query_error_ = esp_wifi_get_mac(WIFI_IF_STA, mac.data());
            if (last_query_error_ != ESP_OK) return std::nullopt;
            char rendered[24U]{};
            std::snprintf(rendered, sizeof(rendered), "%X-%X-%X-%X-%X-%X",
                          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            return rendered;
        }
        return parameter <= 7U
                   ? interface_ipv4("WIFI_STA_DEF", parameter - 5U,
                                    last_query_error_)
                   : std::nullopt;
    }

    std::optional<std::string> access_point_parameter(
        std::uint8_t parameter) override {
        wifi_config_t config{};
        if (parameter <= 3U) {
            last_query_error_ = esp_wifi_get_config(WIFI_IF_AP, &config);
            if (last_query_error_ != ESP_OK) return std::nullopt;
        }
        if (parameter == 0U) {
            return bounded_c_string(config.ap.ssid, sizeof(config.ap.ssid),
                                    maximum_query_ssid_size);
        }
        if (parameter == 1U) {
            return bounded_c_string(config.ap.password, sizeof(config.ap.password),
                                    maximum_query_password_size);
        }
        if (parameter == 2U) return std::to_string(config.ap.channel);
        if (parameter == 3U) return std::to_string(config.ap.authmode);
        if (parameter >= 4U && parameter <= 6U) {
            return interface_ipv4("WIFI_AP_DEF", parameter - 4U,
                                  last_query_error_);
        }
        if (parameter == 7U) {
            wifi_sta_list_t stations{};
            last_query_error_ = esp_wifi_ap_get_sta_list(&stations);
            return last_query_error_ == ESP_OK
                       ? std::optional<std::string>(std::to_string(stations.num))
                       : std::nullopt;
        }
        return std::nullopt;
    }

    void report_query_failure(bool station, std::uint8_t parameter) override {
        ESP_LOGW("APP_AP", "M48%d.%u query failed: %s", station ? 2 : 3,
                 static_cast<unsigned>(parameter),
                 esp_err_to_name(last_query_error_));
    }

private:
    esp_err_t last_query_error_ = ESP_FAIL;
};

TargetAccessPointPort command_port;

}  // namespace

void initialize_access_point_commands(
    std::string_view machine_name,
    const firmware::application::AccessPointStartupSettings& settings) {
    std::lock_guard<std::mutex> lock(command_mutex);
    command_state = {};
    command_state.machine_name = std::string(machine_name);
    command_state.fallback_name = std::string(machine_name);
    command_state.saved_channel = settings.saved_channel;
    command_state.password = settings.password;
    command_state.enabled = settings.enabled;
    wifi_config_t config{};
    if (esp_wifi_get_config(WIFI_IF_AP, &config) == ESP_OK) {
        command_state.last_access_point_name = bounded_c_string(
            config.ap.ssid, sizeof(config.ap.ssid), maximum_query_ssid_size);
        command_state.last_channel = config.ap.channel;
    }
}

std::optional<firmware::core::Frame> execute_access_point_command(
    firmware::core::CommandKind kind, firmware::core::BytesView payload) {
    std::lock_guard<std::mutex> lock(command_mutex);
    return firmware::application::AccessPointCommandService::execute(
        kind, payload, command_state, command_port);
}

}  // namespace firmware::target
