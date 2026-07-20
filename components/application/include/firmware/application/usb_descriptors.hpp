// Declares exact USB device and vendor-interface descriptor bytes.
#pragma once

#include "firmware/core/bytes.hpp"

namespace firmware::application {

// Provides immutable descriptors for the ESP32-S3 vendor-specific USB device.
class UsbDescriptors {
public:
    // Returns the 18-byte USB 2.0 device descriptor.
    static core::ByteVector device();

    // Returns the single bus-powered vendor interface and two bulk endpoints.
    static core::ByteVector configuration();

    // Returns the required UTF-16LE string descriptor for a supported index.
    static core::ByteVector string_descriptor(std::uint8_t index);
};

}  // namespace firmware::application
