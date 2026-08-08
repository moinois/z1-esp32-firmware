/** @file @brief Exact USB device and vendor-interface descriptor bytes. */
#pragma once

#include "firmware/core/bytes.hpp"

namespace firmware::application {

/** Builds immutable descriptors for the ESP32-S3 vendor-specific USB device. */
class UsbDescriptors {
public:
    /// Returns the 18-byte USB 2.0 device descriptor.
    static core::ByteVector device();

    /// Returns one bus-powered vendor interface with bulk IN and OUT endpoints.
    static core::ByteVector configuration();

    /** Returns a UTF-16LE string descriptor, including the LANGID at index zero. */
    static core::ByteVector string_descriptor(std::uint8_t index);
};

}  // namespace firmware::application
