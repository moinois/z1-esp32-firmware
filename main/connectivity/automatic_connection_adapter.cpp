/** @file @brief Implements saved-credential loading and bounded reconnect tasks with ESP-IDF. */
#include "automatic_connection_adapter.hpp"

#include "nvs_key_value_adapter.hpp"
#include "wifi_diagnostic_log.hpp"

#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <algorithm>
#include <cstring>

namespace firmware::target {
namespace {

constexpr char automatic_task_name[] = "wifi_auto_connect";
constexpr char reconnect_task_name[] = "wifi_reconnect";
constexpr std::uint32_t task_stack_size = 3072U;
constexpr std::uint32_t reconnect_task_stack_size = 6144U;
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
    static_cast<void>(wifi_diagnostic_log().trace("auto.start"));
    runtime_ = &runtime;
    static_cast<void>(firmware::application::AutomaticStationConnection::load_and_schedule(
        runtime, *this));
}

void AutomaticConnectionAdapter::on_station_disconnected() {
    const unsigned retry = runtime_ == nullptr ? 0U
                                               : runtime_->automatic_retry_number;
    static_cast<void>(wifi_diagnostic_log().trace(
        "auto.disconnect_event retry=" + std::to_string(retry)));
    if (runtime_ == nullptr) {
        return;
    }
    bool expected = false;
    if (!reconnect_task_active_.compare_exchange_strong(expected, true)) {
        static_cast<void>(wifi_diagnostic_log().trace(
            "auto.disconnect_event.coalesced"));
        return;
    }
    const BaseType_t result = xTaskCreate(
        &AutomaticConnectionAdapter::reconnect_task, reconnect_task_name,
        reconnect_task_stack_size, this, task_priority, nullptr);
    if (result != pdPASS) {
        reconnect_task_active_.store(false);
        static_cast<void>(wifi_diagnostic_log().trace(
            "auto.reconnect_task.create_failed"));
    }
}

firmware::application::StoredString AutomaticConnectionAdapter::read_string(
    std::string_view name_space, std::string_view key) {
    static_cast<void>(wifi_diagnostic_log().trace(
        "auto.read namespace=" + std::string(name_space) + " key=" +
        std::string(key)));
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
    static_cast<void>(wifi_diagnostic_log().trace(
        "auto.apply_config ssid_length=" + std::to_string(ssid_size) +
        " password_length=" + std::to_string(password_size)));
    wifi_config.sta.scan_method = WIFI_FAST_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    if (esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK) {
        return {false, "set_mode"};
    }
    const esp_err_t result = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    static_cast<void>(wifi_diagnostic_log().trace(
        result == ESP_OK ? "auto.apply_config.ok" : "auto.apply_config.error"));
    return api_result(result, "set_config");
}

bool AutomaticConnectionAdapter::schedule_automatic_connection(
    std::uint32_t delay_milliseconds_value) {
    static_cast<void>(wifi_diagnostic_log().trace(
        "auto.schedule delay_ms=" + std::to_string(delay_milliseconds_value)));
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
    static_cast<void>(wifi_diagnostic_log().trace("auto.connect_request"));
    return api_result(esp_wifi_connect(), "connect");
}

void AutomaticConnectionAdapter::delay_milliseconds(std::uint32_t duration) {
    static_cast<void>(wifi_diagnostic_log().trace(
        "auto.delay ms=" + std::to_string(duration)));
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

void AutomaticConnectionAdapter::reconnect_task(void* context) {
    auto* adapter = static_cast<AutomaticConnectionAdapter*>(context);
    bool reconnecting = false;
    if (adapter->runtime_ != nullptr) {
        reconnecting =
            firmware::application::AutomaticStationConnection::handle_disconnect(
                *adapter->runtime_, *adapter);
        static_cast<void>(wifi_diagnostic_log().trace(
            std::string("auto.disconnect_result reconnecting=") +
            (reconnecting ? "true" : "false") + " retry=" +
            std::to_string(adapter->runtime_->automatic_retry_number)));
    }
    adapter->reconnect_task_active_.store(false);
    vTaskDelete(nullptr);
}

}  // namespace firmware::target
