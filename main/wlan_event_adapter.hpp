// Declares ESP-IDF Wi-Fi/IP event registration for discovery lifecycle updates.
#pragma once

namespace firmware::application {
class BleProvisioning;
}

namespace firmware::target {

class AutomaticConnectionAdapter;

class WlanEventAdapter {
public:
    // Connects station disconnect events to the automatic retry policy.
    void set_automatic_connection(AutomaticConnectionAdapter* adapter);

    // Connects Wi-Fi station events to the active BLUFI provisioning session.
    void set_ble_provisioning(firmware::application::BleProvisioning* provisioning);

    // Returns the active provisioning policy for the event thunk.
    firmware::application::BleProvisioning* ble_provisioning() const;

    // Returns the configured automatic connection adapter for the event thunk.
    AutomaticConnectionAdapter* automatic_connection() const;

    // Registers station association and disconnection event callbacks.
    void start();

private:
    AutomaticConnectionAdapter* automatic_connection_ = nullptr;
    firmware::application::BleProvisioning* ble_provisioning_ = nullptr;
};

}  // namespace firmware::target
