// Implements byte-exact common framing without exceptions or target dependencies.
#include "firmware/core/frame.hpp"

#include "firmware/core/crc.hpp"

#include <algorithm>
#include <array>
#include <limits>

namespace firmware::core {
namespace {

constexpr std::array<std::uint8_t, 2> sync_bytes{0x86, 0x68};

void discard_prefix(ByteVector& bytes, std::size_t count) {
    const auto end = bytes.begin() + static_cast<std::ptrdiff_t>(std::min(count, bytes.size()));
    bytes.erase(bytes.begin(), end);
}

}  // namespace

ByteVector encode_frame(const Frame& frame) {
    if (frame.payload.size() > std::numeric_limits<std::uint16_t>::max() - 3U) {
        return {};
    }

    const auto length = static_cast<std::uint16_t>(frame.payload.size() + 3U);
    ByteVector bytes{sync_bytes[0], sync_bytes[1], static_cast<std::uint8_t>(length >> 8U),
                     static_cast<std::uint8_t>(length), frame.type};
    bytes.insert(bytes.end(), frame.payload.begin(), frame.payload.end());

    const auto crc = crc16_ccitt({bytes.data() + 2, bytes.size() - 2});
    bytes.insert(bytes.end(), {static_cast<std::uint8_t>(crc >> 8U), static_cast<std::uint8_t>(crc), 0x55, 0xAA});
    return bytes;
}

std::vector<Frame> StreamDecoder::push(BytesView input) {
    buffer_.insert(buffer_.end(), input.begin(), input.end());
    std::vector<Frame> result;

    for (;;) {
        const auto found = std::search(buffer_.begin(), buffer_.end(), sync_bytes.begin(), sync_bytes.end());
        if (found == buffer_.end()) {
            const bool keep_sync = !buffer_.empty() && buffer_.back() == sync_bytes[0];
            buffer_.assign(keep_sync ? 1U : 0U, sync_bytes[0]);
            break;
        }

        discard_prefix(buffer_, static_cast<std::size_t>(found - buffer_.begin()));
        if (buffer_.size() < 4U) {
            break;
        }

        const auto data_length = static_cast<std::uint16_t>((buffer_[2] << 8U) | buffer_[3]);
        const std::size_t total_size = static_cast<std::size_t>(data_length) + 6U;
        if (data_length < 3U || total_size > policy_.maximum_frame_size) {
            const std::size_t discard_size = policy_.recovery == RecoveryMode::discard_candidate ? 4U : 1U;
            discard_prefix(buffer_, discard_size);
            continue;
        }
        if (buffer_.size() < total_size) {
            break;
        }

        const std::size_t crc_offset = total_size - 4U;
        const auto wire_crc = static_cast<std::uint16_t>((buffer_[crc_offset] << 8U) | buffer_[crc_offset + 1U]);
        const bool valid = buffer_[total_size - 2U] == 0x55U && buffer_[total_size - 1U] == 0xAAU &&
                           wire_crc == crc16_ccitt({buffer_.data() + 2U, crc_offset - 2U});
        if (!valid) {
            const std::size_t discard_size =
                policy_.recovery == RecoveryMode::discard_candidate ? total_size : 1U;
            discard_prefix(buffer_, discard_size);
            continue;
        }

        const auto payload_end = buffer_.begin() + static_cast<std::ptrdiff_t>(crc_offset);
        result.push_back({buffer_[4], ByteVector(buffer_.begin() + 5, payload_end)});
        discard_prefix(buffer_, total_size);
    }
    return result;
}

}  // namespace firmware::core
