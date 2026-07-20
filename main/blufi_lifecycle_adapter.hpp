// Declares the ESP-IDF Bluetooth and BLUFI profile lifecycle adapter.
#pragma once

#include "esp_blufi_api.h"

namespace firmware::target {

// Owns one-time controller, Bluedroid, BLUFI profile, and advertising startup.
class BlufiLifecycleAdapter {
public:
    // Initializes BLE and starts the standard BLUFI advertising profile.
    bool start(const esp_blufi_callbacks_t* callbacks = nullptr);
};

}  // namespace firmware::target
