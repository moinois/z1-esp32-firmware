// Defines common wire parsing and queueing for controller transfer families.
#pragma once

#include "firmware/core/frame.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

namespace firmware::application {

enum class TransferOperation {
    start,
    geometry,
    data,
    complete,
    cancel,
    unknown,
};

// Holds the controller's proposed frame count and frame-data size.
struct TransferGeometry {
    std::uint32_t frame_count;
    std::uint16_t frame_data_size;
};

// Holds a decoded data index and the exact bytes required in its reply.
struct TransferDataRequest {
    std::uint32_t index;
    core::ByteVector wire_index;
};

// Maps the common low nibble of a controller transfer packet to its operation.
TransferOperation transfer_operation(std::uint8_t packet_type);

// Decodes the first six geometry bytes as unsigned big-endian values.
std::optional<TransferGeometry> parse_transfer_geometry(core::BytesView payload);

// Decodes and retains the first four bytes of a transfer data request.
std::optional<TransferDataRequest> parse_transfer_data_request(core::BytesView payload);

// Creates a response using a transfer-family high nibble and response low nibble.
core::Frame make_transfer_reply(std::uint8_t family, std::uint8_t low_nibble,
                                core::ByteVector payload = {});

// Queues one controller transfer family with bounded, paced FIFO processing.
class ControllerTransferInbox {
public:
    // Selects the packet-type high nibble accepted by this inbox.
    explicit ControllerTransferInbox(std::uint8_t family);

    // Accepts one complete frame when family, encoded size, and capacity permit.
    bool enqueue(core::Frame frame);

    // Removes the next frame when the nominal processing interval has elapsed.
    std::optional<core::Frame> take_ready(std::uint64_t now_milliseconds);

    // Returns the number of accepted frames awaiting processing.
    std::size_t pending() const;

private:
    std::uint8_t family_;
    std::deque<core::Frame> frames_;
    std::uint64_t next_process_milliseconds_ = 0U;
};

}  // namespace firmware::application
