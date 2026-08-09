/** @file @brief Implements the BLE controller and standard ESP-IDF BLUFI profile startup. */
#include "blufi_lifecycle_adapter.hpp"

#include "application/provisioning/blufi_advertising.hpp"

#include "esp_blufi.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"

#include <cstdint>
#include <string>

namespace firmware::target {
namespace {

constexpr char tag[] = "BLUFI_LIFE";
std::string active_device_name;
firmware::core::ByteVector active_advertising_data;

esp_ble_adv_params_t blufi_advertising_parameters{
    .adv_int_min = 256U,
    .adv_int_max = 256U,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .peer_addr = {},
    .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// Starts advertising only after ESP-IDF confirms the data is installed.
void blufi_gap_callback(esp_gap_ble_cb_event_t event,
                        esp_ble_gap_cb_param_t* parameter) {
    if (event == ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT) {
        if (parameter != nullptr &&
            parameter->adv_data_raw_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(tag, "Advertising data installation failed: status=%d",
                     parameter->adv_data_raw_cmpl.status);
            return;
        }
        const esp_err_t result = esp_ble_gap_start_advertising(
            &blufi_advertising_parameters);
        if (result != ESP_OK) {
            ESP_LOGE(tag, "Advertising start failed: %s", esp_err_to_name(result));
        } else {
            ESP_LOGI(tag, "Advertising started as %s", active_device_name.c_str());
        }
    }
}

// Applies the product advertising contract.
bool configure_blufi_advertising(std::string_view device_name) {
    active_device_name = device_name;
    const esp_err_t name_result =
        esp_ble_gap_set_device_name(active_device_name.c_str());
    if (name_result != ESP_OK) {
        ESP_LOGE(tag, "Device-name configuration failed: %s",
                 esp_err_to_name(name_result));
        return false;
    }
    // ESP-IDF's structured builder may reorder or silently omit fields when
    // the name approaches 31 bytes. BWF-002 requires an exact conditional
    // layout, so retain and install the portable raw payload verbatim.
    const auto power_level = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_ADV);
    const auto power_dbm = static_cast<std::int8_t>(
        -12 + 3 * static_cast<int>(power_level));
    active_advertising_data = firmware::application::blufi_advertising_data(
        active_device_name, power_dbm);
    const esp_err_t advertising_result = esp_ble_gap_config_adv_data_raw(
        active_advertising_data.data(), active_advertising_data.size());
    if (advertising_result != ESP_OK) {
        ESP_LOGE(tag, "Advertising-data configuration failed: %s",
                 esp_err_to_name(advertising_result));
        return false;
    }
    return true;
}

bool require_ok(esp_err_t result, const char* operation) {
    if (result == ESP_OK) {
        return true;
    }
    ESP_LOGE(tag, "%s failed: %s (0x%x)", operation, esp_err_to_name(result),
             static_cast<unsigned int>(result));
    return false;
}

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

bool restart_blufi_advertising(std::string_view device_name) {
    return configure_blufi_advertising(device_name);
}

bool BlufiLifecycleAdapter::start(const esp_blufi_callbacks_t* callbacks) {
    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
        esp_bt_controller_config_t controller_config = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        if (!require_ok(esp_bt_controller_init(&controller_config),
                        "Bluetooth controller initialization")) {
            return false;
        }
    }
    if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_ENABLED &&
        !require_ok(esp_bt_controller_enable(ESP_BT_MODE_BLE),
                    "Bluetooth controller enable")) {
        return false;
    }
    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_UNINITIALIZED &&
        !require_ok(esp_bluedroid_init(), "Bluedroid initialization")) {
        return false;
    }
    if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED &&
        !require_ok(esp_bluedroid_enable(), "Bluedroid enable")) {
            return false;
    }
    if (!require_ok(esp_ble_gap_register_callback(blufi_gap_callback),
                    "GAP callback registration")) {
        return false;
    }
    const esp_blufi_callbacks_t& selected_callbacks =
        callbacks != nullptr ? *callbacks : lifecycle_callbacks;
    if (!require_ok(esp_blufi_register_callbacks(
                        const_cast<esp_blufi_callbacks_t*>(&selected_callbacks)),
                    "BLUFI callback registration") ||
        !require_ok(esp_blufi_profile_init(), "BLUFI profile initialization")) {
        return false;
    }
    // BleProvisioning starts the product advertisement once lifecycle setup
    // has returned, avoiding two concurrent advertising-data installations.
    return true;
}

}  // namespace firmware::target
