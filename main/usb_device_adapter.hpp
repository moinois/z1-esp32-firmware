// Declares the ESP-IDF TinyUSB vendor-interface lifecycle adapter.
#pragma once

#include "firmware/core/frame.hpp"

namespace firmware::target {

// Installs the native USB full-speed vendor interface and protocol callbacks.
class UsbDeviceAdapter {
public:
    // Starts TinyUSB; failure is reported without stopping other services.
    bool start();
};

// Queues one response for USB transmission when the protocol is active.
bool queue_usb_frame(const firmware::core::Frame& frame);

}  // namespace firmware::target
