// Implements controller firmware transfer independently of concrete I/O adapters.
#include "firmware/application/controller_firmware_transfer.hpp"

#include "firmware/application/controller_transfer.hpp"
#include "firmware/core/protocol_constants.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace firmware::application {
namespace {

constexpr std::string_view firmware_path = "/sd/lpc1768.bin";
constexpr std::uint64_t wait_timeout_milliseconds = 5000U;
constexpr std::uint16_t maximum_frame_data_size = 512U;

// Encodes accepted transfer geometry as six unsigned big-endian bytes.
core::ByteVector encode_geometry(std::uint32_t frame_count, std::uint16_t data_size) {
    return {
        static_cast<std::uint8_t>(frame_count >> 24U),
        static_cast<std::uint8_t>(frame_count >> 16U),
        static_cast<std::uint8_t>(frame_count >> 8U),
        static_cast<std::uint8_t>(frame_count),
        static_cast<std::uint8_t>(data_size >> 8U),
        static_cast<std::uint8_t>(data_size),
    };
}

}  // namespace

void ControllerFirmwareTransfer::handle(const core::Frame& frame,
                                        std::uint64_t now_milliseconds,
                                        ControllerFirmwarePort& port) {
    switch (transfer_operation(frame.type)) {
        case TransferOperation::start:
            handle_start(now_milliseconds, port);
            break;
        case TransferOperation::geometry:
            handle_geometry(frame.payload, now_milliseconds, port);
            break;
        case TransferOperation::data:
            handle_data(frame.payload, port);
            break;
        case TransferOperation::complete:
            finish(FirmwareTransferEvent::completed, port);
            break;
        case TransferOperation::cancel:
            finish(FirmwareTransferEvent::cancelled, port);
            break;
        case TransferOperation::unknown:
            break;
    }
    apply_timeout(now_milliseconds, port);
}

void ControllerFirmwareTransfer::handle_start(std::uint64_t now_milliseconds,
                                              ControllerFirmwarePort& port) {
    if (!port.file_exists(firmware_path)) {
        report_error(port);
        return;
    }

    active_ = true;
    waiting_ = true;
    wait_started_milliseconds_ = now_milliseconds;
    port.publish(FirmwareTransferEvent::started, 0U, frame_count_);
    if (!port.send(make_transfer_reply(core::protocol::firmware_family,
                                       core::protocol::transfer_start))) {
        report_error(port);
    }
}

void ControllerFirmwareTransfer::handle_geometry(core::BytesView payload,
                                                 std::uint64_t now_milliseconds,
                                                 ControllerFirmwarePort& port) {
    const auto proposed = parse_transfer_geometry(payload);
    const auto file_size = port.file_size(firmware_path);
    if (!proposed.has_value() || proposed->frame_data_size == 0U ||
        proposed->frame_data_size > maximum_frame_data_size || !file_size.has_value()) {
        report_error(port);
        return;
    }

    const std::uint64_t blocks =
        (*file_size + proposed->frame_data_size - 1U) / proposed->frame_data_size;
    if (blocks > std::numeric_limits<std::uint32_t>::max()) {
        report_error(port);
        return;
    }

    frame_count_ = static_cast<std::uint32_t>(blocks);
    frame_data_size_ = proposed->frame_data_size;
    if (active_) {
        waiting_ = true;
        wait_started_milliseconds_ = now_milliseconds;
    }
    if (!port.send(make_transfer_reply(core::protocol::firmware_family,
                                       core::protocol::transfer_geometry,
                                       encode_geometry(frame_count_, frame_data_size_)))) {
        report_error(port);
    }
}

void ControllerFirmwareTransfer::handle_data(core::BytesView payload,
                                             ControllerFirmwarePort& port) {
    const auto request = parse_transfer_data_request(payload);
    if (!request.has_value()) {
        report_error(port);
        return;
    }

    const std::uint64_t block = request->index <= 1U ? 0U : request->index - 1U;
    const std::uint64_t offset = block * frame_data_size_;
    auto data = port.read_file(firmware_path, offset, frame_data_size_);
    if (active_) {
        waiting_ = false;
    }
    if (!data.has_value()) {
        report_error(port);
        return;
    }
    if (frame_data_size_ == 0U || data->empty()) {
        return;
    }
    if (data->size() > frame_data_size_) {
        data->resize(frame_data_size_);
    }

    core::ByteVector response = request->wire_index;
    response.insert(response.end(), data->begin(), data->end());
    if (!port.send(make_transfer_reply(core::protocol::firmware_family,
                                       core::protocol::transfer_data,
                                       std::move(response)))) {
        report_error(port);
        return;
    }
    port.publish(FirmwareTransferEvent::progress, request->index, frame_count_);
}

void ControllerFirmwareTransfer::finish(FirmwareTransferEvent event,
                                        ControllerFirmwarePort& port) {
    active_ = false;
    waiting_ = false;
    frame_count_ = 0U;
    frame_data_size_ = 0U;
    port.publish(event, 0U, 0U);
}

void ControllerFirmwareTransfer::report_error(ControllerFirmwarePort& port) {
    port.publish(FirmwareTransferEvent::error, 0U, frame_count_);
    port.send(make_transfer_reply(core::protocol::firmware_family,
                                  core::protocol::transfer_cancel));
}

void ControllerFirmwareTransfer::apply_timeout(std::uint64_t now_milliseconds,
                                               ControllerFirmwarePort& port) {
    const bool timeout_elapsed =
        now_milliseconds - wait_started_milliseconds_ >= wait_timeout_milliseconds;
    if (!active_ || !waiting_ || !timeout_elapsed) {
        return;
    }
    finish(FirmwareTransferEvent::timed_out, port);
}

bool ControllerFirmwareTransfer::active() const {
    return active_;
}

std::uint32_t ControllerFirmwareTransfer::frame_count() const {
    return frame_count_;
}

std::uint16_t ControllerFirmwareTransfer::frame_data_size() const {
    return frame_data_size_;
}

}  // namespace firmware::application
