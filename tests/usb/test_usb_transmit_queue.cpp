// Verifies USB transmit capacity, bounds, and completion-driven FIFO removal.
#include "test.hpp"

#include "firmware/application/usb_transmit_queue.hpp"
#include "firmware/core/file_transfer_limits.hpp"
#include "firmware/core/protocol_constants.hpp"

using firmware::application::UsbTransmitQueue;
using firmware::core::ByteVector;

TEST_CASE(usb_007_queue_accepts_30_frames_and_preserves_fifo_order) {
    UsbTransmitQueue queue;
    for (std::uint8_t value = 0U; value < 30U; ++value) {
        REQUIRE(queue.enqueue(ByteVector{value}));
    }
    REQUIRE(!queue.enqueue(ByteVector{0xffU}));
    REQUIRE_EQ(queue.size(), 30U);
    REQUIRE_EQ(queue.front()->front(), 0U);
    queue.pop_front();
    REQUIRE_EQ(queue.front()->front(), 1U);
}

TEST_CASE(usb_007_queue_rejects_empty_and_oversized_frames) {
    UsbTransmitQueue queue;
    REQUIRE(!queue.enqueue(ByteVector{}));
    REQUIRE(queue.enqueue(ByteVector(UsbTransmitQueue::maximum_frame_size, 0x5aU)));
    REQUIRE(!queue.enqueue(
        ByteVector(UsbTransmitQueue::maximum_frame_size + 1U, 0x5aU)));
}

TEST_CASE(usb_007_queue_accepts_a_maximum_file_data_frame) {
    UsbTransmitQueue queue;
    const std::size_t encoded_file_frame_size =
        firmware::core::file_transfer_limits::data_block_size +
        firmware::core::protocol::big_endian_u32_size +
        firmware::core::protocol::common_frame_overhead;
    REQUIRE(encoded_file_frame_size <= UsbTransmitQueue::maximum_frame_size);
    REQUIRE(queue.enqueue(ByteVector(encoded_file_frame_size, 0x5aU)));
}
