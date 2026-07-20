// Implements SPIFFS registration and formatting through ESP-IDF.
#include "web_volume_adapter.hpp"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"

namespace firmware::target {
namespace {

constexpr char tag[] = "WEB_VOLUME";

}  // namespace

bool WebVolumeAdapter::mount(const application::WebVolumeConfig& config) {
    const esp_vfs_spiffs_conf_t spiffs_config{
        .base_path = config.mount_path.data(),
        .partition_label = nullptr,
        .max_files = config.maximum_open_files,
        .format_if_mount_failed = false,
    };
    const esp_err_t result = esp_vfs_spiffs_register(&spiffs_config);
    if (result != ESP_OK) {
        ESP_LOGW(tag, "SPIFFS mount failed: %s", esp_err_to_name(result));
        return false;
    }
    ESP_LOGI(tag, "SPIFFS mounted at %s", config.mount_path.data());
    return true;
}

bool WebVolumeAdapter::format(const application::WebVolumeConfig& config) {
    esp_vfs_spiffs_unregister(nullptr);
    const esp_err_t result = esp_spiffs_format(nullptr);
    if (result != ESP_OK) {
        ESP_LOGW(tag, "SPIFFS format failed for %s: %s", config.mount_path.data(),
                 esp_err_to_name(result));
        return false;
    }
    return true;
}

}  // namespace firmware::target
