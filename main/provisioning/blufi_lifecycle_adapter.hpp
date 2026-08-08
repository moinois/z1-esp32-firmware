/** @file @brief Declares the ESP-IDF Bluetooth and BLUFI profile lifecycle adapter. */
#pragma once

#include "esp_blufi_api.h"

#include <string_view>

namespace firmware::target {

/// Reapplies the fixed BLUFI identity and advertising contract after disconnect.
bool restart_blufi_advertising(std::string_view device_name);

/// Owns one-time controller, Bluedroid, BLUFI profile, and advertising startup.
class BlufiLifecycleAdapter {
public:
    /// Initializes BLE and starts the standard BLUFI advertising profile.
    bool start(const esp_blufi_callbacks_t* callbacks = nullptr);
};

}  // namespace firmware::target
