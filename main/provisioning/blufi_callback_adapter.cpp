// Implements the narrow ESP-IDF-to-application BLUFI callback translation.
#include "blufi_callback_adapter.hpp"

#include "firmware/core/bytes.hpp"

#include <array>
#include <algorithm>
#include "esp_crc.h"
#include <cstring>

namespace firmware::target {

BlufiCallbackAdapter* BlufiCallbackAdapter::active_instance_ = nullptr;
BlufiCryptoAdapter BlufiCallbackAdapter::crypto_;
firmware::application::BlufiSecurityContext
    BlufiCallbackAdapter::security_(BlufiCallbackAdapter::crypto_);
firmware::core::ByteVector BlufiCallbackAdapter::negotiation_output_;

BlufiCallbackAdapter::BlufiCallbackAdapter(
    firmware::application::BleProvisioning& provisioning)
    : provisioning_(provisioning) {
    active_instance_ = this;
    callbacks_.event_cb = &BlufiCallbackAdapter::event_callback;
    callbacks_.negotiate_data_handler = &BlufiCallbackAdapter::negotiate_data_handler;
    callbacks_.encrypt_func = &BlufiCallbackAdapter::encrypt_data;
    callbacks_.decrypt_func = &BlufiCallbackAdapter::decrypt_data;
    callbacks_.checksum_func = &BlufiCallbackAdapter::checksum_data;
}

const esp_blufi_callbacks_t& BlufiCallbackAdapter::callbacks() const {
    return callbacks_;
}

void BlufiCallbackAdapter::event_callback(esp_blufi_cb_event_t event,
                                          esp_blufi_cb_param_t* parameter) {
    if (active_instance_ == nullptr) {
        return;
    }
    auto& provisioning = active_instance_->provisioning_;
    switch (event) {
        case ESP_BLUFI_EVENT_BLE_CONNECT:
            security_.create();
            provisioning.client_connected();
            return;
        case ESP_BLUFI_EVENT_BLE_DISCONNECT:
            security_.destroy();
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
            if (parameter == nullptr) {
                return;
            }
            provisioning.set_operation_mode(
                static_cast<std::uint8_t>(parameter->wifi_mode.op_mode));
            return;
        case ESP_BLUFI_EVENT_RECV_STA_BSSID:
            {
                if (parameter == nullptr) {
                    return;
                }
                std::array<std::uint8_t, 6U> bssid{};
                std::copy(std::begin(parameter->sta_bssid.bssid),
                          std::end(parameter->sta_bssid.bssid), bssid.begin());
                provisioning.station_associated(bssid, {});
            }
            return;
        case ESP_BLUFI_EVENT_RECV_STA_SSID:
            if (parameter == nullptr) {
                return;
            }
            provisioning.receive_ssid(firmware::core::BytesView(
                parameter->sta_ssid.ssid,
                static_cast<std::size_t>(parameter->sta_ssid.ssid_len)));
            return;
        case ESP_BLUFI_EVENT_RECV_STA_PASSWD:
            if (parameter == nullptr) {
                return;
            }
            provisioning.receive_password(firmware::core::BytesView(
                parameter->sta_passwd.passwd,
                static_cast<std::size_t>(parameter->sta_passwd.passwd_len)));
            return;
        case ESP_BLUFI_EVENT_REPORT_ERROR:
            if (parameter == nullptr) {
                return;
            }
            provisioning.receive_error(
                static_cast<std::uint8_t>(parameter->report_error.state));
            return;
        case ESP_BLUFI_EVENT_RECV_CUSTOM_DATA:
            if (parameter == nullptr) {
                return;
            }
            provisioning.receive_custom_data(firmware::core::BytesView(
                parameter->custom_data.data,
                parameter->custom_data.data_len));
            return;
        default:
            return;
    }
}

void BlufiCallbackAdapter::negotiate_data_handler(
    std::uint8_t* data, int length, std::uint8_t** output,
    int* output_length, bool* needs_free) {
    if (data == nullptr || length < 0 || output == nullptr ||
        output_length == nullptr || needs_free == nullptr) {
        return;
    }
    security_.receive_negotiation(
        firmware::core::BytesView(data, static_cast<std::size_t>(length)));
    negotiation_output_ = crypto_.take_negotiation_response().value_or(
        firmware::core::ByteVector{});
    *output = negotiation_output_.empty() ? nullptr : negotiation_output_.data();
    *output_length = static_cast<int>(negotiation_output_.size());
    *needs_free = false;
}

int BlufiCallbackAdapter::encrypt_data(std::uint8_t sequence,
                                       std::uint8_t* data, int length) {
    if (data == nullptr || length < 0) {
        return -1;
    }
    const auto result = security_.crypt(
        sequence, firmware::core::BytesView(data, static_cast<std::size_t>(length)), true);
    if (!result.has_value()) {
        return -1;
    }
    std::memcpy(data, result->data(), result->size());
    return length;
}

int BlufiCallbackAdapter::decrypt_data(std::uint8_t sequence,
                                       std::uint8_t* data, int length) {
    if (data == nullptr || length < 0) {
        return -1;
    }
    const auto result = security_.crypt(
        sequence, firmware::core::BytesView(data, static_cast<std::size_t>(length)), false);
    if (!result.has_value()) {
        return -1;
    }
    std::memcpy(data, result->data(), result->size());
    return length;
}

std::uint16_t BlufiCallbackAdapter::checksum_data(
    std::uint8_t, std::uint8_t* data, int length) {
    if (data == nullptr || length < 0) {
        return 0U;
    }
    return esp_crc16_be(0U, data, static_cast<std::size_t>(length));
}

}  // namespace firmware::target
