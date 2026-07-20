// Starts the mainboard services in the normative order defined by BOOT-010 through BOOT-019.
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "canopen_target_service.hpp"
#include "http_server_adapter.hpp"
#include "web_volume_adapter.hpp"
#include "storage_retention_adapter.hpp"
#include "sd_card_adapter.hpp"
#include "controller_command_loop.hpp"
#include "tcp_control_adapter.hpp"
#include "wlan_event_adapter.hpp"
#include "connectivity_startup_adapter.hpp"
#include "automatic_connection_adapter.hpp"
#include "blufi_lifecycle_adapter.hpp"
#include "blufi_provisioning_adapter.hpp"
#include "blufi_callback_adapter.hpp"
#include "diagnostic_capture_adapter.hpp"
#include "runtime_counter_task.hpp"

#include "firmware/application/web_volume_startup.hpp"
#include "firmware/application/connectivity_startup.hpp"

#include <cstdint>

namespace {
constexpr gpio_num_t heartbeat_gpio = GPIO_NUM_0;
constexpr std::uint32_t heartbeat_period_milliseconds = 1000U;
constexpr std::uint32_t heartbeat_stack_size = 2048U;
constexpr UBaseType_t heartbeat_priority = 3U;
constexpr char tag[] = "MAIN";

// Initializes NVS with the erase-and-retry recovery required during early boot.
bool initialize_persistent_store() {
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(tag, "NVS分区需要擦除，正在擦除...");
        result = nvs_flash_erase();
        if (result == ESP_OK) {
            result = nvs_flash_init();
        }
    } else if (result != ESP_OK) {
        ESP_LOGE(tag, "NVS初始化失败: %s (0x%x)", esp_err_to_name(result), static_cast<unsigned>(result));
        ESP_LOGW(tag, "尝试擦除NVS分区并重新初始化...");
        result = nvs_flash_erase();
        if (result == ESP_OK) {
            result = nvs_flash_init();
        }
    }
    if (result != ESP_OK) {
        ESP_LOGE(tag, "NVS初始化仍然失败，系统无法继续运行");
        return false;
    }
    ESP_LOGI(tag, "NVS初始化成功");
    return true;
}

// Toggles the active-high heartbeat output once per second for the lifetime of the firmware.
void heartbeat_task(void*) {
    bool high = true;
    gpio_set_level(heartbeat_gpio, high);
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(heartbeat_period_milliseconds));
        high = !high;
        gpio_set_level(heartbeat_gpio, high);
    }
}

// Starts the nonfatal heartbeat service and leaves later services independent of its result.
void start_heartbeat() {
    const gpio_config_t config{.pin_bit_mask = 1ULL << heartbeat_gpio,
                               .mode = GPIO_MODE_OUTPUT,
                               .pull_up_en = GPIO_PULLUP_DISABLE,
                               .pull_down_en = GPIO_PULLDOWN_DISABLE,
                               .intr_type = GPIO_INTR_DISABLE};
    if (gpio_config(&config) != ESP_OK) {
        return;
    }
    xTaskCreate(heartbeat_task, "heartbeat", heartbeat_stack_size, nullptr,
                heartbeat_priority, nullptr);
}
}  // namespace

extern "C" void app_main() {
    if (!initialize_persistent_store()) {
        esp_restart();
    }
    start_heartbeat();
    if (esp_netif_init() != ESP_OK || esp_event_loop_create_default() != ESP_OK) {
        esp_restart();
    }
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&wifi_config) != ESP_OK) {
        esp_restart();
    }
    static firmware::target::ConnectivityStartupAdapter connectivity_adapter;
    if (!firmware::application::ConnectivityStartup::start(connectivity_adapter,
                                                           "espressif")) {
        ESP_LOGE(tag, "Connectivity startup failed; restarting");
        esp_restart();
    }
    static firmware::target::BlufiProvisioningAdapter blufi_port;
    static firmware::application::StationRuntime blufi_station_runtime;
    static firmware::application::BleProvisioning blufi_provisioning(
        blufi_station_runtime, blufi_port);
    static firmware::target::BlufiCallbackAdapter blufi_callbacks(blufi_provisioning);
    static firmware::target::BlufiLifecycleAdapter blufi_lifecycle;
    if (!blufi_lifecycle.start(&blufi_callbacks.callbacks()) ||
        !blufi_provisioning.start()) {
        ESP_LOGW(tag, "BLUFI lifecycle did not start");
    }
    static firmware::target::WebVolumeAdapter web_volume_adapter;
    static firmware::application::WebVolumeStartup web_volume_startup;
    web_volume_startup.start(web_volume_adapter);
    static firmware::target::HttpServerAdapter http_server;
    http_server.start();
    static firmware::target::CanopenTargetService canopen_service;
    canopen_service.start();
    static firmware::target::StorageRetentionAdapter retention_adapter;
    retention_adapter.start();
    static firmware::target::SdCardAdapter sd_card_adapter;
    sd_card_adapter.start();
    static firmware::target::DiagnosticCaptureAdapter diagnostic_capture;
    diagnostic_capture.start();
    static firmware::target::RuntimeCounterTask runtime_counter_task;
    runtime_counter_task.start();
    static firmware::target::ControllerCommandLoop controller_command_loop;
    controller_command_loop.start();
    static firmware::target::TcpControlAdapter tcp_control;
    static firmware::application::StationRuntime automatic_station_runtime;
    static firmware::target::AutomaticConnectionAdapter automatic_connection;
    automatic_connection.start(automatic_station_runtime);
    static firmware::target::WlanEventAdapter wlan_events;
    wlan_events.set_automatic_connection(&automatic_connection);
    wlan_events.set_ble_provisioning(&blufi_provisioning);
    wlan_events.start();
    tcp_control.start();
}
