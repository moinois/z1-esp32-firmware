// Declares ESP-IDF BLUFI event translation into the portable provisioning policy.
#pragma once

#include "esp_blufi_api.h"

#include "firmware/application/ble_provisioning.hpp"

namespace firmware::target {

// Owns the callback table while forwarding only supported product events.
class BlufiCallbackAdapter {
public:
    explicit BlufiCallbackAdapter(firmware::application::BleProvisioning& provisioning);

    // Returns the stable callback table consumed by esp_blufi_register_callbacks.
    const esp_blufi_callbacks_t& callbacks() const;

private:
    static void event_callback(esp_blufi_cb_event_t event,
                               esp_blufi_cb_param_t* parameter);
    static BlufiCallbackAdapter* active_instance_;
    firmware::application::BleProvisioning& provisioning_;
    esp_blufi_callbacks_t callbacks_{};
};

}  // namespace firmware::target
