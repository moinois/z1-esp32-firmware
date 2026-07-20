// Declares ESP-IDF Wi-Fi/IP event registration for discovery lifecycle updates.
#pragma once

namespace firmware::target {

class WlanEventAdapter {
public:
    // Registers station association and disconnection event callbacks.
    void start();
};

}  // namespace firmware::target
