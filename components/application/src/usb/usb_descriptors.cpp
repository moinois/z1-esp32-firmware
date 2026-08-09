/** @file @brief Implements the fixed USB descriptors required by the firmware specification. */
#include "application/usb/usb_descriptors.hpp"

#include <string_view>

namespace firmware::application {
namespace {

core::ByteVector utf16_string(std::string_view text) {
    core::ByteVector result{static_cast<std::uint8_t>(2U + text.size() * 2U), 0x03U};
    for (const char character : text) {
        result.push_back(static_cast<std::uint8_t>(character));
        result.push_back(0U);
    }
    return result;
}

}  // namespace

core::ByteVector UsbDescriptors::device() {
    return {0x12U, 0x01U, 0x00U, 0x02U, 0x00U, 0x00U, 0x00U, 0x40U,
            0x3aU, 0x30U, 0x02U, 0x40U, 0x00U, 0x01U, 0x01U, 0x02U,
            0x03U, 0x01U};
}

core::ByteVector UsbDescriptors::configuration() {
    return {0x09U, 0x02U, 0x20U, 0x00U, 0x01U, 0x01U, 0x00U, 0x80U,
            0xfaU, 0x09U, 0x04U, 0x00U, 0x00U, 0x02U, 0xffU, 0x00U,
            0x00U, 0x00U, 0x07U, 0x05U, 0x01U, 0x02U, 0x40U, 0x00U,
            0x00U, 0x07U, 0x05U, 0x81U, 0x02U, 0x40U, 0x00U, 0x00U};
}

core::ByteVector UsbDescriptors::string_descriptor(std::uint8_t index) {
    if (index == 0U) {
        return {0x04U, 0x03U, 0x09U, 0x04U};
    }
    if (index == 1U) return utf16_string("Espressif");
    if (index == 2U) return utf16_string("MakeraZ1 (USB)");
    if (index == 3U) return utf16_string("123456");
    return {};
}

}  // namespace firmware::application
