// Implements saved-credential loading and bounded reconnect tasks with ESP-IDF.
#include "automatic_connection_adapter.hpp"

#include "nvs_key_value_adapter.hpp"

#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <algorithm>
#include <cstring>

namespace firmware::target {
namespace {

constexpr char credential_namespace[] = "wifi_config";
constexpr char automatic_task_name[] = "wifi_auto_connect";
constexpr std::uint32_t task_stack_size = 3072U;
constexpr UBaseType_t task_priority = 4U;

firmware::application::StationApiResult api_result(esp_err_t result,
                                                    const char* operation) {
    return result == ESP_OK
        ? firmware::application::StationApiResult{true, {}}
        : firmware::application::StationApiResult{false, operation};
}

}  // namespace

void AutomaticConnectionAdapter::start(
    firmware::application::StationRuntime& runtime) {
    runtime_ = &runtime;
    static_cast<void>(firmware::application::AutomaticStationConnection::load_and_schedule(
        runtime, *this));
}

void AutomaticConnectionAdapter::on_station_disconnected() {
    if (runtime_ != nullptr) {
        static_cast<void>(firmware::application::AutomaticStationConnection::handle_disconnect(
            *runtime_, *this));
    }
}

firmware::application::StoredString AutomaticConnectionAdapter::read_string(
    std::string_view name_space, std::string_view key) {
    const NvsStringRead result = NvsKeyValueAdapter{}.read_string(name_space, key);
    if (result.state == NvsReadState::found) {
        return {firmware::application::StoredStringState::found, result.value, {}};
    }
    if (result.state == NvsReadState::missing) {
        return {firmware::application::StoredStringState::missing, {}, {}};
    }
    return {firmware::application::StoredStringState::failure, {}, "nvs_read"};
}

firmware::application::StationApiResult AutomaticConnectionAdapter::apply_station_config(
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

bool AutomaticConnectionAdapter::schedule_automatic_connection(
    std::uint32_t delay_milliseconds_value) {
    scheduled_delay_milliseconds_ = delay_milliseconds_value;
    const BaseType_t result = xTaskCreate(&AutomaticConnectionAdapter::delayed_connect_task,
                                          automatic_task_name, task_stack_size, this,
                                          task_priority, nullptr);
    if (result != pdPASS) {
        return false;
    }
    // The task performs the delay so app_main remains non-blocking.
    return true;
}

firmware::application::StationApiResult AutomaticConnectionAdapter::request_connect() {
    return api_result(esp_wifi_connect(), "connect");
}

void AutomaticConnectionAdapter::delay_milliseconds(std::uint32_t duration) {
    vTaskDelay(pdMS_TO_TICKS(duration));
}

void AutomaticConnectionAdapter::delayed_connect_task(void* context) {
    auto* adapter = static_cast<AutomaticConnectionAdapter*>(context);
    adapter->delay_milliseconds(adapter->scheduled_delay_milliseconds_);
    if (adapter->runtime_ != nullptr) {
        static_cast<void>(firmware::application::AutomaticStationConnection::begin_scheduled(
            *adapter->runtime_, *adapter));
    }
    vTaskDelete(nullptr);
}

}  // namespace firmware::target
