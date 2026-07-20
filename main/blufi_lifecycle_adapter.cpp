// Implements the BLE controller and standard ESP-IDF BLUFI profile startup.
#include "blufi_lifecycle_adapter.hpp"

#include "esp_blufi.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"

#include <cstdint>

namespace firmware::target {
namespace {

constexpr char blufi_device_name[] = "BLUFI_DEVICE";
std::uint8_t blufi_service_uuid[] = {0xffU, 0xffU};

// Applies the product advertising contract before the BLUFI profile starts.
bool configure_blufi_advertising() {
    if (esp_ble_gap_set_device_name(blufi_device_name) != ESP_OK) {
        return false;
    }
    esp_ble_adv_data_t advertising{};
    advertising.set_scan_rsp = false;
    advertising.include_name = true;
    advertising.include_txpower = true;
    advertising.min_interval = 0x0006;
    advertising.max_interval = 0x0010;
    advertising.service_uuid_len = sizeof(blufi_service_uuid);
    advertising.p_service_uuid = blufi_service_uuid;
    advertising.flag = ESP_BLE_ADV_FLAG_GEN_DISC |
                       ESP_BLE_ADV_FLAG_BREDR_NOT_SPT;
    return esp_ble_gap_config_adv_data(&advertising) == ESP_OK;
}

// Selects public, undirected advertising on all three primary channels.
bool start_blufi_advertising() {
    esp_ble_adv_params_t parameters{};
    parameters.adv_int_min = 256U;
    parameters.adv_int_max = 256U;
    parameters.adv_type = ADV_TYPE_IND;
    parameters.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
    parameters.channel_map = ADV_CHNL_ALL;
    parameters.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
    return esp_ble_gap_start_advertising(&parameters) == ESP_OK;
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

bool restart_blufi_advertising() {
    return configure_blufi_advertising() && start_blufi_advertising();
}

bool BlufiLifecycleAdapter::start(const esp_blufi_callbacks_t* callbacks) {
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
    const esp_blufi_callbacks_t& selected_callbacks =
        callbacks != nullptr ? *callbacks : lifecycle_callbacks;
    if (esp_blufi_register_callbacks(
            const_cast<esp_blufi_callbacks_t*>(&selected_callbacks)) != ESP_OK ||
        esp_blufi_profile_init() != ESP_OK) {
        return false;
    }
    esp_blufi_adv_start();
    // The profile helper installs its legacy defaults; replace them with the
    // product advertisement and explicit 160 ms public advertising policy.
    return restart_blufi_advertising();
}

}  // namespace firmware::target
