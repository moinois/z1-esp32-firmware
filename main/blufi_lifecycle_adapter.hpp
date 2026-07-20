// Declares the ESP-IDF Bluetooth and BLUFI profile lifecycle adapter.
#pragma once

namespace firmware::target {

// Owns one-time controller, Bluedroid, BLUFI profile, and advertising startup.
class BlufiLifecycleAdapter {
public:
    // Initializes BLE and starts the standard BLUFI advertising profile.
    bool start();
};

}  // namespace firmware::target
