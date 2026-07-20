// Implements the BLE controller and standard ESP-IDF BLUFI profile startup.
#include "blufi_lifecycle_adapter.hpp"

#include "esp_blufi.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"

#include <cstdint>

namespace firmware::target {
namespace {

// BLUFI requires a callback table even before product callbacks are composed.
void lifecycle_event_callback(esp_blufi_cb_event_t, esp_blufi_cb_param_t*) {}

esp_blufi_callbacks_t lifecycle_callbacks{
    .event_cb = lifecycle_event_callback,
    .negotiate_data_handler = nullptr,
    .encrypt_func = nullptr,
    .decrypt_func = nullptr,
    .checksum_func = nullptr,
};

}  // namespace

bool BlufiLifecycleAdapter::start() {
    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
        esp_bt_controller_config_t controller_config = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        if (esp_bt_controller_init(&controller_config) != ESP_OK) {
            return false;
        }
    }
    if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_ENABLED &&
        esp_bt_controller_enable(ESP_BT_MODE_BLE) != ESP_OK) {
        return false;
    }
    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_UNINITIALIZED &&
        esp_bluedroid_init() != ESP_OK) {
        return false;
    }
    if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED &&
        esp_bluedroid_enable() != ESP_OK) {
            return false;
    }
    if (esp_blufi_register_callbacks(&lifecycle_callbacks) != ESP_OK ||
        esp_blufi_profile_init() != ESP_OK) {
        return false;
    }
    esp_blufi_adv_start();
    return true;
}

}  // namespace firmware::target
