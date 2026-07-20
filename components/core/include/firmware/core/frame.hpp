// Defines common frame encoding and transport-specific incremental decoding.
#pragma once
#include "firmware/core/bytes.hpp"
#include "firmware/core/protocol_constants.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>
namespace firmware::core {
struct Frame {
    std::uint8_t type = 0;
    ByteVector payload;
    bool operator==(const Frame& other) const {
        return type == other.type && payload == other.payload;
    }
};
enum class RecoveryMode { scan_inside_candidate, discard_candidate };
struct StreamPolicy {
    std::size_t maximum_frame_size;
    RecoveryMode recovery;
    static constexpr StreamPolicy controller_uart() {
        return {protocol::controller_maximum_frame_size,
                RecoveryMode::scan_inside_candidate};
    }

    static constexpr StreamPolicy tcp() {
        return {protocol::host_maximum_frame_size,
                RecoveryMode::scan_inside_candidate};
    }

    static constexpr StreamPolicy usb() {
        return {protocol::host_maximum_frame_size,
                RecoveryMode::discard_candidate};
    }
};
ByteVector encode_frame(const Frame& frame);
class StreamDecoder {
public:
    explicit StreamDecoder(StreamPolicy policy) : policy_(policy) {}
    std::vector<Frame> push(BytesView input);
    void reset() {
        buffer_.clear();
    }
private:
    StreamPolicy policy_;
    ByteVector buffer_;
};
}  // namespace firmware::core
