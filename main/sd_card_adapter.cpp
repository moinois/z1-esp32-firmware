// Implements SDMMC slot-1 mounting with the specified GPIO and card-detect policy.
#include "sd_card_adapter.hpp"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_default_configs.h"

#include "firmware/application/sd_card_lifecycle.hpp"

#include <optional>

namespace firmware::target {
namespace {

constexpr gpio_num_t card_detect_gpio = GPIO_NUM_2;
constexpr char tag[] = "SD";

class EspSdPort final : public firmware::application::SdCardPort {
public:
    bool card_inserted() override {
        return gpio_get_level(card_detect_gpio) == 0;
    }

    bool mount(const firmware::application::SdMountConfig& config) override {
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        host.max_freq_khz = 20000U;
        sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
        slot.width = 4U;
        slot.clk = GPIO_NUM_39;
        slot.cmd = GPIO_NUM_38;
        slot.d0 = GPIO_NUM_40;
        slot.d1 = GPIO_NUM_1;
        slot.d2 = GPIO_NUM_45;
        slot.d3 = GPIO_NUM_48;
        esp_vfs_fat_mount_config_t mount_config{
            .format_if_mount_failed = config.format_if_mount_fails,
            .max_files = static_cast<int>(config.maximum_open_files),
            .allocation_unit_size = config.allocation_unit_size,
            .disk_status_check_enable = false,
        };
        return esp_vfs_fat_sdmmc_mount(config.mount_path.data(), &host, &slot,
                                       &mount_config, &card_) == ESP_OK;
    }

    void start_logging() override {}
    void stop_and_drain_logging() override {}

    bool unmount() override {
        return esp_vfs_fat_sdcard_unmount("/sd", card_) == ESP_OK;
    }

    std::optional<std::uint64_t> total_bytes() override { return volume_bytes(true); }
    std::optional<std::uint64_t> free_bytes() override { return volume_bytes(false); }

private:
    std::optional<std::uint64_t> volume_bytes(bool total) {
        std::uint64_t total_bytes = 0U;
        std::uint64_t free_bytes = 0U;
        if (esp_vfs_fat_info("/sd", &total_bytes, &free_bytes) != ESP_OK) {
            return std::nullopt;
        }
        return total ? total_bytes : free_bytes;
    }

    sdmmc_card_t* card_ = nullptr;
};

void sd_monitor_task(void*) {
    gpio_config_t detect_config{};
    detect_config.pin_bit_mask = 1ULL << card_detect_gpio;
    detect_config.mode = GPIO_MODE_INPUT;
    detect_config.pull_up_en = GPIO_PULLUP_ENABLE;
    detect_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    detect_config.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&detect_config);
    EspSdPort port;
    firmware::application::SdCardLifecycle lifecycle;
    const std::uint64_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
    lifecycle.start(start, port);
    for (;;) {
        const std::uint64_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        lifecycle.poll(now, port);
        vTaskDelay(pdMS_TO_TICKS(50U));
    }
}

}  // namespace

void SdCardAdapter::start() {
    xTaskCreate(sd_monitor_task, "sd_monitor", 4096U, nullptr, 4U, nullptr);
}

}  // namespace firmware::target
