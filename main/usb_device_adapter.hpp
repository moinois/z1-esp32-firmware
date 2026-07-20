// Declares the ESP-IDF TinyUSB vendor-interface lifecycle adapter.
#pragma once

namespace firmware::target {

// Installs the native USB full-speed vendor interface and protocol callbacks.
class UsbDeviceAdapter {
public:
    // Starts TinyUSB; failure is reported without stopping other services.
    bool start();
};

}  // namespace firmware::target
