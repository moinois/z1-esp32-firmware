/** @file @brief Implements the exact GPIO0 heartbeat electrical configuration. */
#include "heartbeat_adapter.hpp"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace firmware::target {
namespace {

constexpr gpio_num_t heartbeat_gpio = GPIO_NUM_0;

}  // namespace

bool EspHeartbeatAdapter::configure_output() {
    const gpio_config_t config{
        .pin_bit_mask = 1ULL << heartbeat_gpio,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return gpio_config(&config) == ESP_OK;
}

void EspHeartbeatAdapter::set_level(bool high) {
    static_cast<void>(gpio_set_level(heartbeat_gpio, high ? 1U : 0U));
}

void EspHeartbeatAdapter::delay_milliseconds(std::uint32_t duration) {
    vTaskDelay(pdMS_TO_TICKS(duration));
}

}  // namespace firmware::target
