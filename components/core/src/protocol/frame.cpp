/** @file @brief Implements byte-exact common framing without exceptions or target dependencies. */
#include "firmware/core/frame.hpp"

#include "firmware/core/crc.hpp"

#include <algorithm>
#include <array>
#include <limits>

namespace firmware::core {
namespace {

constexpr std::array<std::uint8_t, 2> sync_bytes{0x86, 0x68};
constexpr std::array<std::uint8_t, 2> tail_bytes{0x55, 0xAA};
constexpr std::size_t length_field_offset = sync_bytes.size();
constexpr std::size_t frame_type_offset = 4U;
constexpr std::size_t payload_offset = frame_type_offset + 1U;
constexpr std::size_t bytes_after_data_length = 6U;
constexpr std::size_t trailer_size = 4U;
constexpr std::uint16_t minimum_data_length = 3U;
constexpr std::uint16_t non_payload_data_length = 3U;

void discard_prefix(ByteVector& bytes, std::size_t count) {
    const auto end = bytes.begin() + static_cast<std::ptrdiff_t>(std::min(count, bytes.size()));
    bytes.erase(bytes.begin(), end);
}

bool is_controller_update_type(std::uint8_t type) {
    return (type & protocol::family_mask) == protocol::firmware_family;
}

ByteVector encode_with_crc(const Frame& frame, bool controller_transport) {
    if (frame.payload.size() >
        std::numeric_limits<std::uint16_t>::max() - non_payload_data_length) {
        return {};
    }
    const auto length = static_cast<std::uint16_t>(
        frame.payload.size() + non_payload_data_length);
    ByteVector bytes{
        sync_bytes[0], sync_bytes[1],
        static_cast<std::uint8_t>(length >> 8U),
        static_cast<std::uint8_t>(length), frame.type,
    };
    bytes.insert(bytes.end(), frame.payload.begin(), frame.payload.end());
    const BytesView crc_input{bytes.data() + length_field_offset,
                              bytes.size() - length_field_offset};
    const auto crc = controller_transport && is_controller_update_type(frame.type)
                         ? crc16_controller_update(crc_input)
                         : crc16_ccitt(crc_input);
    bytes.insert(bytes.end(),
                 {static_cast<std::uint8_t>(crc >> 8U),
                  static_cast<std::uint8_t>(crc), tail_bytes[0], tail_bytes[1]});
    return bytes;
}

}  // namespace

ByteVector encode_frame(const Frame& frame) {
    return encode_with_crc(frame, false);
}

ByteVector encode_controller_frame(const Frame& frame) {
    return encode_with_crc(frame, true);
}

std::vector<Frame> StreamDecoder::push(BytesView input) {
    buffer_.insert(buffer_.end(), input.begin(), input.end());
    std::vector<Frame> result;

    for (;;) {
        const auto found = std::search(buffer_.begin(), buffer_.end(),
                                       sync_bytes.begin(), sync_bytes.end());
        if (found == buffer_.end()) {
            const bool keep_sync = !buffer_.empty() && buffer_.back() == sync_bytes[0];
            buffer_.assign(keep_sync ? 1U : 0U, sync_bytes[0]);
            break;
        }

        discard_prefix(buffer_, static_cast<std::size_t>(found - buffer_.begin()));
        if (buffer_.size() < frame_type_offset) {
            break;
        }

        const auto data_length = static_cast<std::uint16_t>(
            (buffer_[length_field_offset] << 8U) |
            buffer_[length_field_offset + 1U]);
        const std::size_t total_size =
            static_cast<std::size_t>(data_length) + bytes_after_data_length;
        if (data_length < minimum_data_length ||
            total_size > policy_.maximum_frame_size) {
            const std::size_t discard_size =
                policy_.recovery == RecoveryMode::discard_candidate
                    ? frame_type_offset
                    : 1U;
            discard_prefix(buffer_, discard_size);
            continue;
        }
        if (buffer_.size() < total_size) {
            break;
        }

        const std::size_t crc_offset = total_size - trailer_size;
        const auto wire_crc = static_cast<std::uint16_t>(
            (buffer_[crc_offset] << 8U) | buffer_[crc_offset + 1U]);
        const bool valid_tail =
            buffer_[total_size - tail_bytes.size()] == tail_bytes[0] &&
            buffer_[total_size - 1U] == tail_bytes[1];
        const BytesView crc_input{buffer_.data() + length_field_offset,
                                  crc_offset - length_field_offset};
        const auto expected_crc =
            policy_.controller_update_crc &&
                    is_controller_update_type(buffer_[frame_type_offset])
                ? crc16_controller_update(crc_input)
                : crc16_ccitt(crc_input);
        const bool valid_crc = wire_crc == expected_crc;
        if (!valid_tail || !valid_crc) {
            const std::size_t discard_size =
                policy_.recovery == RecoveryMode::discard_candidate ? total_size : 1U;
            discard_prefix(buffer_, discard_size);
            continue;
        }

        const auto payload_end = buffer_.begin() + static_cast<std::ptrdiff_t>(crc_offset);
        result.push_back({buffer_[frame_type_offset],
                          ByteVector(buffer_.begin() + payload_offset, payload_end)});
        discard_prefix(buffer_, total_size);
    }
    return result;
}

}  // namespace firmware::core
