// Implements TinyUSB vendor callbacks over the transport-neutral USB policies.
#include "usb_device_adapter.hpp"

#include "tinyusb.h"
#include "tusb.h"
#include "class/vendor/vendor_device.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "firmware/application/usb_descriptors.hpp"
#include "firmware/application/usb_protocol_state.hpp"
#include "firmware/application/usb_transmit_progress.hpp"
#include "firmware/application/recording_commands.hpp"
#include "firmware/application/serial_number.hpp"
#include "nvs_key_value_adapter.hpp"
#include "runtime_operation_capacity.hpp"
#include "recording_request_state.hpp"
#include "firmware/core/text.hpp"
#include "firmware/core/frame.hpp"
#include "controller_command_loop.hpp"
#include "firmware_update_adapter.hpp"
#include "firmware/application/controller_snapshots.hpp"
#include "runtime_status_adapter.hpp"

#include <array>
#include <esp_timer.h>

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
RecordingRequestState recording_state;

class UsbSerialPort final : public firmware::application::SerialNumberPort {
public:
    bool admit_operation(std::uint32_t wait_milliseconds) override {
        return admit_runtime_operation(wait_milliseconds);
    }

    firmware::application::SerialNumberRead read_serial(
        std::string_view name_space, std::string_view key) override {
        NvsKeyValueAdapter nvs;
        const auto result = nvs.read_string(name_space, key);
        if (result.state == NvsReadState::found) {
            return {firmware::application::SerialNumberReadResult::success,
                    result.value};
        }
        if (result.state == NvsReadState::missing) {
            return {firmware::application::SerialNumberReadResult::missing_key,
                    {}};
        }
        return {firmware::application::SerialNumberReadResult::failure, {}};
    }

    bool write_serial(std::string_view name_space, std::string_view key,
                      std::string_view value) override {
        NvsKeyValueAdapter nvs;
        return nvs.write_string(name_space, key, value);
    }

    void complete_operation() override {
        complete_runtime_operation();
    }

    void send_response(std::uint8_t type, std::string_view payload) override {
        const firmware::core::Frame response{
            type, firmware::core::ByteVector(payload.begin(), payload.end())};
        const auto encoded = firmware::core::encode_frame(response);
        if (!encoded.empty()) {
            static_cast<void>(protocol_state.transmit_queue().enqueue(encoded));
        }
    }
};

UsbSerialPort serial_port;

void usb_transmit_task(void*) {
    firmware::application::UsbTransmitProgress progress;
    const firmware::core::ByteVector* tracked_frame = nullptr;
    for (;;) {
        if (protocol_state.can_send()) {
            const auto* frame = protocol_state.transmit_queue().front();
            if (frame != tracked_frame) {
                tracked_frame = frame;
                progress.begin(static_cast<std::uint64_t>(
                    esp_timer_get_time() / 1000LL));
            }
            if (frame != nullptr && tud_vendor_write_available() >= frame->size()) {
                const std::uint32_t written =
                    tud_vendor_write(frame->data(), frame->size());
                if (written == frame->size()) {
                    tud_vendor_flush();
                    protocol_state.transmit_queue().pop_front();
                    progress.clear();
                    tracked_frame = nullptr;
                } else if (written > 0U) {
                    progress.record_progress(static_cast<std::uint64_t>(
                        esp_timer_get_time() / 1000LL));
                }
            }
            if (frame != nullptr && progress.expired(static_cast<std::uint64_t>(
                    esp_timer_get_time() / 1000LL))) {
                protocol_state.transmit_queue().pop_front();
                progress.clear();
                tracked_frame = nullptr;
            }
        } else {
            tracked_frame = nullptr;
            progress.clear();
        }
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
}

void consume_received_bytes(const std::uint8_t* bytes, std::size_t size) {
    if (bytes == nullptr || size == 0U) return;
    firmware::application::UsbReceiveStaging& staging =
        protocol_state.receive_staging();
    if (!staging.stage({bytes, size})) return;
    const auto staged = staging.take();
    for (const auto& frame : decoder.push(staged)) {
        protocol_state.valid_frame_received();
        if (frame.type == firmware::core::protocol::general_command) {
            const auto match = firmware::core::recognize_command(frame.payload);
            if (match.kind == firmware::core::CommandKind::record_start ||
                match.kind == firmware::core::CommandKind::record_stop) {
                const auto result = firmware::application::handle_recording_command(
                    match.kind, recording_state.requested());
                recording_state.set_requested(result.requested);
                const auto response = firmware::core::encode_frame(result.response);
                if (!response.empty()) {
                    static_cast<void>(protocol_state.transmit_queue().enqueue(response));
                }
                continue;
            }
            if (match.kind == firmware::core::CommandKind::serial_get ||
                match.kind == firmware::core::CommandKind::serial_set) {
                const std::string_view command(
                    reinterpret_cast<const char*>(frame.payload.data()),
                    frame.payload.size());
                firmware::application::SerialNumberService service(serial_port);
                if (match.kind == firmware::core::CommandKind::serial_get) {
                    service.handle_get(command);
                } else {
                    service.handle_set(command);
                }
                continue;
            }
            if (match.kind == firmware::core::CommandKind::upgrade ||
                match.kind == firmware::core::CommandKind::reset) {
                request_firmware_update_processing();
                continue;
            }
            if (match.kind == firmware::core::CommandKind::diagnose) {
                const auto response =
                    shared_controller_snapshots().diagnostic_reply(0);
                if (response.has_value()) {
                    static_cast<void>(protocol_state.transmit_queue().enqueue(
                        firmware::core::encode_frame(*response)));
                }
                continue;
            }
            if (match.kind == firmware::core::CommandKind::version) {
                const auto response =
                    shared_controller_snapshots().version_reply();
                static_cast<void>(protocol_state.transmit_queue().enqueue(
                    firmware::core::encode_frame(response)));
                continue;
            }
        }
        // Forward complete USB frames through the controller-owned UART path.
        // Local USB command routing will be added without changing this boundary.
        static_cast<void>(enqueue_controller_frame(frame));
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
    xTaskCreate(usb_transmit_task, "usb_tx", 4096U, nullptr, 4U, nullptr);
    return true;
}

bool queue_usb_frame(const firmware::core::Frame& frame) {
    const auto encoded = firmware::core::encode_frame(frame);
    if (encoded.empty()) return false;
    return protocol_state.transmit_queue().enqueue(encoded);
}

}  // namespace firmware::target
