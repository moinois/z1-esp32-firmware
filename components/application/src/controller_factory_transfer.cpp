// Implements factory-data record selection and lifecycle behavior.
#include "firmware/application/controller_factory_transfer.hpp"

#include "firmware/application/controller_transfer.hpp"

#include <utility>

namespace firmware::application {
namespace {

constexpr std::string_view factory_path = "/sd/factory.ini";
constexpr std::size_t input_chunk_size = 255U;
constexpr std::uint16_t maximum_frame_data_size = 512U;
constexpr std::size_t maximum_record_size = 132U;

// Reports whether a raw factory chunk contributes to geometry and selection.
bool eligible_record(const core::ByteVector& chunk) {
    return chunk.size() > 2U && chunk.front() != '#';
}

// Encodes accepted factory geometry as six big-endian bytes.
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

void ControllerFactoryTransfer::handle(const core::Frame& frame, ControllerFactoryPort& port) {
    switch (transfer_operation(frame.type)) {
        case TransferOperation::start:
            handle_start(port);
            break;
        case TransferOperation::geometry:
            handle_geometry(frame.payload, port);
            break;
        case TransferOperation::data:
            handle_data(frame.payload, port);
            break;
        case TransferOperation::complete:
            finish(true, port);
            break;
        case TransferOperation::cancel:
            finish(false, port);
            break;
        case TransferOperation::unknown:
            break;
    }
}

void ControllerFactoryTransfer::handle_start(ControllerFactoryPort& port) {
    const bool acknowledgement_sent = port.send(make_transfer_reply(0xE0U, 1U));
    const bool available = port.file_exists(factory_path);
    if (available) {
        active_ = true;
    }
    if (!acknowledgement_sent || !available) {
        report_error(port);
    }
}

void ControllerFactoryTransfer::handle_geometry(core::BytesView payload,
                                                ControllerFactoryPort& port) {
    const auto proposed = parse_transfer_geometry(payload);
    if (!proposed.has_value() || proposed->frame_data_size == 0U ||
        proposed->frame_data_size > maximum_frame_data_size) {
        report_error(port);
        return;
    }
    const auto chunks = port.read_chunks(factory_path, input_chunk_size);
    if (!chunks.has_value()) {
        report_error(port);
        return;
    }

    std::uint32_t count = 0U;
    for (const auto& chunk : *chunks) {
        if (eligible_record(chunk)) {
            ++count;
        }
    }
    frame_count_ = count;
    frame_data_size_ = proposed->frame_data_size;
    if (!port.send(make_transfer_reply(
            0xE0U, 2U, encode_geometry(frame_count_, frame_data_size_)))) {
        report_error(port);
    }
}

void ControllerFactoryTransfer::handle_data(core::BytesView payload,
                                            ControllerFactoryPort& port) {
    const auto request = parse_transfer_data_request(payload);
    if (!request.has_value()) {
        report_error(port);
        return;
    }
    if (request->index == 0U || frame_data_size_ == 0U) {
        return;
    }
    const auto chunks = port.read_chunks(factory_path, input_chunk_size);
    if (!chunks.has_value()) {
        report_error(port);
        return;
    }

    std::uint32_t eligible_index = 0U;
    for (const auto& chunk : *chunks) {
        if (!eligible_record(chunk)) {
            continue;
        }
        ++eligible_index;
        if (eligible_index != request->index) {
            continue;
        }
        if (chunk.size() > maximum_record_size || chunk.size() > frame_data_size_) {
            return;
        }
        core::ByteVector response = request->wire_index;
        response.insert(response.end(), chunk.begin(), chunk.end());
        if (!port.send(make_transfer_reply(0xE0U, 3U, std::move(response)))) {
            report_error(port);
        }
        return;
    }
}

void ControllerFactoryTransfer::finish(bool remove_file, ControllerFactoryPort& port) {
    active_ = false;
    frame_count_ = 0U;
    frame_data_size_ = 0U;
    if (remove_file) {
        static_cast<void>(port.remove_file(factory_path));
    }
}

void ControllerFactoryTransfer::report_error(ControllerFactoryPort& port) {
    port.send(make_transfer_reply(0xE0U, 5U));
}

bool ControllerFactoryTransfer::active() const {
    return active_;
}

std::uint32_t ControllerFactoryTransfer::frame_count() const {
    return frame_count_;
}

std::uint16_t ControllerFactoryTransfer::frame_data_size() const {
    return frame_data_size_;
}

}  // namespace firmware::application
