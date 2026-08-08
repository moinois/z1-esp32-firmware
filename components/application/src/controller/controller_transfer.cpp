/** @file @brief Implements common controller-transfer parsing and paced family inboxes. */
#include "firmware/application/controller_transfer.hpp"

#include "firmware/core/protocol_constants.hpp"

#include <utility>

namespace firmware::application {
namespace {

constexpr std::size_t maximum_pending_frames = 32U;
constexpr std::size_t maximum_encoded_size = core::protocol::controller_maximum_item_size;
constexpr std::size_t encoded_frame_overhead = core::protocol::common_frame_overhead;
constexpr std::size_t geometry_size =
    core::protocol::big_endian_u32_size + core::protocol::big_endian_u16_size;
constexpr std::uint64_t processing_interval_milliseconds = 10U;

// Decodes an unsigned 32-bit big-endian integer from four available bytes.
std::uint32_t decode_u32(core::BytesView bytes) {
    return (static_cast<std::uint32_t>(bytes[0]) << 24U) |
           (static_cast<std::uint32_t>(bytes[1]) << 16U) |
           (static_cast<std::uint32_t>(bytes[2]) << 8U) |
           static_cast<std::uint32_t>(bytes[3]);
}

}  // namespace

TransferOperation transfer_operation(std::uint8_t packet_type) {
    switch (packet_type & core::protocol::operation_mask) {
        case core::protocol::transfer_start:
            return TransferOperation::start;
        case core::protocol::transfer_geometry:
            return TransferOperation::geometry;
        case core::protocol::transfer_data:
            return TransferOperation::data;
        case core::protocol::transfer_complete:
            return TransferOperation::complete;
        case core::protocol::transfer_cancel:
            return TransferOperation::cancel;
        default:
            return TransferOperation::unknown;
    }
}

std::optional<TransferGeometry> parse_transfer_geometry(core::BytesView payload) {
    if (payload.size() < geometry_size) {
        return std::nullopt;
    }
    const std::uint16_t data_size =
        static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(payload[core::protocol::big_endian_u32_size]) << 8U) |
            payload[core::protocol::big_endian_u32_size + 1U]);
    return TransferGeometry{
        decode_u32({payload.data(), core::protocol::big_endian_u32_size}),
        data_size,
    };
}

std::optional<TransferDataRequest> parse_transfer_data_request(core::BytesView payload) {
    if (payload.size() < core::protocol::big_endian_u32_size) {
        return std::nullopt;
    }
    return TransferDataRequest{
        decode_u32({payload.data(), core::protocol::big_endian_u32_size}),
        core::ByteVector(payload.begin(),
                         payload.begin() + core::protocol::big_endian_u32_size),
    };
}

core::ByteVector encode_transfer_geometry(std::uint32_t frame_count,
                                          std::uint16_t frame_data_size) {
    return {
        static_cast<std::uint8_t>(frame_count >> 24U),
        static_cast<std::uint8_t>(frame_count >> 16U),
        static_cast<std::uint8_t>(frame_count >> 8U),
        static_cast<std::uint8_t>(frame_count),
        static_cast<std::uint8_t>(frame_data_size >> 8U),
        static_cast<std::uint8_t>(frame_data_size),
    };
}

core::Frame make_transfer_reply(std::uint8_t family, std::uint8_t low_nibble,
                                core::ByteVector payload) {
    return {
        core::protocol::family_packet(family, low_nibble),
        std::move(payload),
    };
}

ControllerTransferInbox::ControllerTransferInbox(std::uint8_t family)
    : family_(family & core::protocol::family_mask) {}

bool ControllerTransferInbox::enqueue(core::Frame frame) {
    const bool correct_family = (frame.type & core::protocol::family_mask) == family_;
    const bool size_allowed = frame.payload.size() <= maximum_encoded_size - encoded_frame_overhead;
    if (!correct_family || !size_allowed || frames_.size() >= maximum_pending_frames) {
        return false;
    }
    frames_.push_back(std::move(frame));
    return true;
}

std::optional<core::Frame> ControllerTransferInbox::take_ready(std::uint64_t now_milliseconds) {
    if (frames_.empty() || now_milliseconds < next_process_milliseconds_) {
        return std::nullopt;
    }
    core::Frame frame = std::move(frames_.front());
    frames_.pop_front();
    next_process_milliseconds_ = now_milliseconds + processing_interval_milliseconds;
    return frame;
}

std::size_t ControllerTransferInbox::pending() const {
    return frames_.size();
}

}  // namespace firmware::application
