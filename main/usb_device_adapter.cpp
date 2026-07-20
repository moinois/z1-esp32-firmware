// Implements TinyUSB vendor callbacks over the transport-neutral USB policies.
#include "usb_device_adapter.hpp"

#include "tinyusb.h"
#include "tusb.h"
#include "class/vendor/vendor_device.h"

#include "esp_log.h"

#include "firmware/application/usb_descriptors.hpp"
#include "firmware/application/usb_protocol_state.hpp"
#include "firmware/core/frame.hpp"

#include <array>

namespace firmware::target {
namespace {

constexpr char tag[] = "usb";
constexpr std::array<std::uint8_t, 18> device_descriptor{
    0x12U, 0x01U, 0x00U, 0x02U, 0x00U, 0x00U, 0x00U, 0x40U,
    0x3aU, 0x30U, 0x02U, 0x40U, 0x00U, 0x01U, 0x01U, 0x02U,
    0x03U, 0x01U};
constexpr std::array<std::uint8_t, 32> configuration_descriptor{
    0x09U, 0x02U, 0x20U, 0x00U, 0x01U, 0x01U, 0x00U, 0x80U,
    0xfaU, 0x09U, 0x04U, 0x00U, 0x00U, 0x02U, 0xffU, 0x00U,
    0x00U, 0x00U, 0x07U, 0x05U, 0x01U, 0x02U, 0x40U, 0x00U,
    0x00U, 0x07U, 0x05U, 0x81U, 0x02U, 0x40U, 0x00U, 0x00U};
const char* string_descriptors[] = {"Espressif", "MakeraZ1 (USB)", "123456"};
firmware::application::UsbProtocolState protocol_state;
firmware::core::StreamDecoder decoder(firmware::core::StreamPolicy::usb());

void consume_received_bytes(const std::uint8_t* bytes, std::size_t size) {
    if (bytes == nullptr || size == 0U) return;
    firmware::application::UsbReceiveStaging& staging =
        protocol_state.receive_staging();
    if (!staging.stage({bytes, size})) return;
    const auto staged = staging.take();
    for (const auto& frame : decoder.push(staged)) {
        (void)frame;
        protocol_state.valid_frame_received();
    }
}

}  // namespace

extern "C" void tud_mount_cb(void) {
    protocol_state.enumerated();
}

extern "C" void tud_umount_cb(void) {
    decoder.reset();
    protocol_state.disconnected();
}

extern "C" void tud_vendor_rx_cb(uint8_t index, const uint8_t*, uint16_t) {
    if (index != 0U) return;
    std::array<std::uint8_t, 512> buffer{};
    const std::uint32_t count = tud_vendor_read(buffer.data(), buffer.size());
    consume_received_bytes(buffer.data(), count);
}

extern "C" void tud_vendor_tx_cb(uint8_t, uint32_t) {}

bool UsbDeviceAdapter::start() {
    const tinyusb_config_t configuration{
        .device_descriptor = reinterpret_cast<const tusb_desc_device_t*>(
            device_descriptor.data()),
        .string_descriptor = string_descriptors,
        .string_descriptor_count = 3,
        .external_phy = false,
        .configuration_descriptor = configuration_descriptor.data(),
        .self_powered = false,
        .vbus_monitor_io = -1,
    };
    const esp_err_t result = tinyusb_driver_install(&configuration);
    if (result != ESP_OK) {
        ESP_LOGW(tag, "TinyUSB installation failed: %s", esp_err_to_name(result));
        return false;
    }
    return true;
}

}  // namespace firmware::target
