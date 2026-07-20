// Starts the mainboard services in the normative order defined by BOOT-010 through BOOT-019.
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "canopen_target_service.hpp"
#include "http_server_adapter.hpp"
#include "web_volume_adapter.hpp"
#include "storage_retention_adapter.hpp"
#include "sd_card_adapter.hpp"
#include "diagnostic_capture_adapter.hpp"

#include "firmware/application/web_volume_startup.hpp"

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
}
