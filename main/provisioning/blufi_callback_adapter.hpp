/** @file @brief Declares ESP-IDF BLUFI event translation into the portable provisioning policy. */
#pragma once

#include "esp_blufi_api.h"

#include "application/provisioning/ble_provisioning.hpp"
#include "blufi_crypto_adapter.hpp"
#include "application/provisioning/blufi_security.hpp"

namespace firmware::target {

/// Owns the callback table while forwarding only supported product events.
class BlufiCallbackAdapter {
public:
    explicit BlufiCallbackAdapter(firmware::application::BleProvisioning& provisioning);

    /// Returns the stable callback table consumed by esp_blufi_register_callbacks.
    const esp_blufi_callbacks_t& callbacks() const;

private:
    static void event_callback(esp_blufi_cb_event_t event,
                               esp_blufi_cb_param_t* parameter);
    static void negotiate_data_handler(std::uint8_t* data, int length,
                                       std::uint8_t** output, int* output_length,
                                       bool* needs_free);
    static int encrypt_data(std::uint8_t sequence, std::uint8_t* data, int length);
    static int decrypt_data(std::uint8_t sequence, std::uint8_t* data, int length);
    static std::uint16_t checksum_data(std::uint8_t sequence,
                                       std::uint8_t* data, int length);
    static BlufiCallbackAdapter* active_instance_;
    firmware::application::BleProvisioning& provisioning_;
    esp_blufi_callbacks_t callbacks_{};
    static BlufiCryptoAdapter crypto_;
    static firmware::application::BlufiSecurityContext security_;
    static firmware::core::ByteVector negotiation_output_;
};

}  // namespace firmware::target
