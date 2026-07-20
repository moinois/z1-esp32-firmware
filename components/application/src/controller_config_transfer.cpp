// Implements configuration record selection and aggregation for the controller.
#include "firmware/application/controller_config_transfer.hpp"

#include "firmware/application/controller_transfer.hpp"

#include <algorithm>
#include <utility>

namespace firmware::application {
namespace {

constexpr std::string_view configuration_path = "/sd/config.txt";
constexpr std::size_t input_chunk_size = 255U;
constexpr std::uint16_t response_data_size = 512U;
constexpr std::size_t minimum_remaining_space = 80U;

// Encodes retained configuration geometry as six big-endian bytes.
core::ByteVector encode_geometry(std::uint32_t frame_count) {
    return {
        static_cast<std::uint8_t>(frame_count >> 24U),
        static_cast<std::uint8_t>(frame_count >> 16U),
        static_cast<std::uint8_t>(frame_count >> 8U),
        static_cast<std::uint8_t>(frame_count),
        0x02U,
        0x00U,
    };
}

// Reports whether a chunk is selectable as controller configuration content.
bool eligible_record(const core::ByteVector& chunk, bool final_chunk) {
    if (chunk.size() <= 2U || chunk.front() == '#' || chunk.front() == '*') {
        return false;
    }
    return chunk.back() == '\n' || final_chunk;
}

// Applies the controller's maximum-length transformation to one record.
core::ByteVector transform_record(const core::ByteVector& record) {
    if (record.size() <= 64U) {
        return record;
    }
    core::ByteVector transformed(record.begin(), record.begin() + 62);
    transformed.push_back('\n');
    transformed.push_back(0U);
    return transformed;
}

}  // namespace

void ControllerConfigTransfer::handle(const core::Frame& frame, ControllerConfigPort& port) {
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
        case TransferOperation::cancel:
            finish();
            break;
        case TransferOperation::unknown:
            break;
    }
}

void ControllerConfigTransfer::handle_start(ControllerConfigPort& port) {
    const bool acknowledgement_sent = port.send(make_transfer_reply(0xD0U, 1U));
    const bool available = port.file_exists(configuration_path);
    if (available) {
        active_ = true;
    }
    if (!acknowledgement_sent || !available) {
        report_error(port);
    }
}

void ControllerConfigTransfer::handle_geometry(core::BytesView payload,
                                               ControllerConfigPort& port) {
    if (!parse_transfer_geometry(payload).has_value()) {
        report_error(port);
        return;
    }
    const auto chunks = port.read_chunks(configuration_path, input_chunk_size);
    if (!chunks.has_value()) {
        report_error(port);
        return;
    }

    frame_count_ = static_cast<std::uint32_t>(std::count_if(
        chunks->begin(), chunks->end(), [](const core::ByteVector& chunk) {
            return chunk.size() > 2U;
        }));
    frame_data_size_ = response_data_size;
    if (!port.send(make_transfer_reply(0xD0U, 2U, encode_geometry(frame_count_)))) {
        report_error(port);
    }
}

void ControllerConfigTransfer::handle_data(core::BytesView payload,
                                           ControllerConfigPort& port) {
    const auto request = parse_transfer_data_request(payload);
    if (!request.has_value()) {
        report_error(port);
        return;
    }
    if (frame_data_size_ == 0U) {
        return;
    }
    const auto chunks = port.read_chunks(configuration_path, input_chunk_size);
    if (!chunks.has_value()) {
        report_error(port);
        return;
    }

    std::vector<core::ByteVector> records;
    for (std::size_t index = 0U; index < chunks->size(); ++index) {
        if (eligible_record((*chunks)[index], index + 1U == chunks->size())) {
            records.push_back(transform_record((*chunks)[index]));
        }
    }

    const std::size_t skip = request->index <= 1U ? 0U : request->index;
    core::ByteVector data;
    for (std::size_t index = skip; index < records.size(); ++index) {
        data.insert(data.end(), records[index].begin(), records[index].end());
        if (response_data_size - data.size() < minimum_remaining_space) {
            break;
        }
    }
    if (data.empty()) {
        return;
    }
    if (data.size() < response_data_size) {
        data.push_back('\n');
    }

    core::ByteVector response = request->wire_index;
    response.insert(response.end(), data.begin(), data.end());
    if (!port.send(make_transfer_reply(0xD0U, 3U, std::move(response)))) {
        report_error(port);
    }
}

void ControllerConfigTransfer::finish() {
    active_ = false;
    frame_count_ = 0U;
    frame_data_size_ = 0U;
}

void ControllerConfigTransfer::report_error(ControllerConfigPort& port) {
    port.send(make_transfer_reply(0xD0U, 5U));
}

bool ControllerConfigTransfer::active() const {
    return active_;
}

std::uint32_t ControllerConfigTransfer::frame_count() const {
    return frame_count_;
}

std::uint16_t ControllerConfigTransfer::frame_data_size() const {
    return frame_data_size_;
}

}  // namespace firmware::application
