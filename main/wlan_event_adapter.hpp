// Declares ESP-IDF Wi-Fi/IP event registration for discovery lifecycle updates.
#pragma once

namespace firmware::target {

class AutomaticConnectionAdapter;

class WlanEventAdapter {
public:
    // Connects station disconnect events to the automatic retry policy.
    void set_automatic_connection(AutomaticConnectionAdapter* adapter);

    // Returns the configured automatic connection adapter for the event thunk.
    AutomaticConnectionAdapter* automatic_connection() const;

    // Registers station association and disconnection event callbacks.
    void start();

private:
    AutomaticConnectionAdapter* automatic_connection_ = nullptr;
};

}  // namespace firmware::target
