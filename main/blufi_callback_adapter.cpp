// Implements the narrow ESP-IDF-to-application BLUFI callback translation.
#include "blufi_callback_adapter.hpp"

#include "firmware/core/bytes.hpp"

#include <array>
#include <algorithm>

namespace firmware::target {

BlufiCallbackAdapter* BlufiCallbackAdapter::active_instance_ = nullptr;

BlufiCallbackAdapter::BlufiCallbackAdapter(
    firmware::application::BleProvisioning& provisioning)
    : provisioning_(provisioning) {
    active_instance_ = this;
    callbacks_.event_cb = &BlufiCallbackAdapter::event_callback;
    callbacks_.negotiate_data_handler = nullptr;
    callbacks_.encrypt_func = nullptr;
    callbacks_.decrypt_func = nullptr;
    callbacks_.checksum_func = nullptr;
}

const esp_blufi_callbacks_t& BlufiCallbackAdapter::callbacks() const {
    return callbacks_;
}

void BlufiCallbackAdapter::event_callback(esp_blufi_cb_event_t event,
                                          esp_blufi_cb_param_t* parameter) {
    if (active_instance_ == nullptr || parameter == nullptr) {
        return;
    }
    auto& provisioning = active_instance_->provisioning_;
    switch (event) {
        case ESP_BLUFI_EVENT_BLE_CONNECT:
            provisioning.client_connected();
            return;
        case ESP_BLUFI_EVENT_BLE_DISCONNECT:
            provisioning.client_disconnected();
            return;
        case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP:
            provisioning.connect_station();
            return;
        case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
            provisioning.disconnect_station();
            return;
        case ESP_BLUFI_EVENT_GET_WIFI_STATUS:
            provisioning.request_status();
            return;
        case ESP_BLUFI_EVENT_GET_WIFI_LIST:
            provisioning.request_wifi_list();
            return;
        case ESP_BLUFI_EVENT_SET_WIFI_OPMODE:
            provisioning.set_operation_mode(
                static_cast<std::uint8_t>(parameter->wifi_mode.op_mode));
            return;
        case ESP_BLUFI_EVENT_RECV_STA_BSSID:
            {
                std::array<std::uint8_t, 6U> bssid{};
                std::copy(std::begin(parameter->sta_bssid.bssid),
                          std::end(parameter->sta_bssid.bssid), bssid.begin());
                provisioning.station_associated(bssid, {});
            }
            return;
        case ESP_BLUFI_EVENT_RECV_STA_SSID:
            provisioning.receive_ssid(firmware::core::BytesView(
                parameter->sta_ssid.ssid,
                static_cast<std::size_t>(parameter->sta_ssid.ssid_len)));
            return;
        case ESP_BLUFI_EVENT_RECV_STA_PASSWD:
            provisioning.receive_password(firmware::core::BytesView(
                parameter->sta_passwd.passwd,
                static_cast<std::size_t>(parameter->sta_passwd.passwd_len)));
            return;
        case ESP_BLUFI_EVENT_REPORT_ERROR:
            provisioning.receive_error(
                static_cast<std::uint8_t>(parameter->report_error.state));
            return;
        case ESP_BLUFI_EVENT_RECV_CUSTOM_DATA:
            provisioning.receive_custom_data(firmware::core::BytesView(
                parameter->custom_data.data,
                parameter->custom_data.data_len));
            return;
        default:
            return;
    }
}

}  // namespace firmware::target
