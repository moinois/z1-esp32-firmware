// Verifies USB transmit capacity, bounds, and completion-driven FIFO removal.
#include "test.hpp"

#include "application/usb/usb_transmit_queue.hpp"
#include "core/filesystem/file_transfer_limits.hpp"
#include "core/protocol/protocol_constants.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using firmware::application::UsbTransmitQueue;
using firmware::core::ByteVector;

TEST_CASE(usb_007_queue_accepts_30_frames_and_preserves_fifo_order) {
    UsbTransmitQueue queue;
    for (std::uint8_t value = 0U; value < 30U; ++value) {
        REQUIRE(queue.enqueue(ByteVector{value}));
    }
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

TEST_CASE(usb_007_concurrent_producers_cannot_exceed_the_thirty_frame_limit) {
    UsbTransmitQueue queue;
    std::atomic<std::size_t> accepted{0U};
    std::vector<std::thread> producers;
    for (std::size_t producer = 0U; producer < 4U; ++producer) {
        producers.emplace_back([&queue, &accepted, producer] {
            for (std::size_t frame = 0U; frame < 20U; ++frame) {
                if (queue.enqueue(ByteVector{
                        static_cast<std::uint8_t>(producer),
                        static_cast<std::uint8_t>(frame)})) {
                    ++accepted;
                }
            }
        });
    }
    for (auto& producer : producers) {
        producer.join();
    }

    REQUIRE_EQ(accepted.load(), UsbTransmitQueue::maximum_items);
    REQUIRE_EQ(queue.size(), 0U);
    REQUIRE(queue.failed());
}

TEST_CASE(usb_007_submission_waits_for_capacity_released_before_deadline) {
    UsbTransmitQueue queue;
    for (std::size_t index = 0U; index < UsbTransmitQueue::maximum_items; ++index) {
        REQUIRE(queue.enqueue(ByteVector{static_cast<std::uint8_t>(index)}));
    }
    std::thread consumer([&queue] {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        queue.pop_front();
    });
    REQUIRE(queue.enqueue(ByteVector{0xffU}, std::chrono::milliseconds(50)));
    consumer.join();
    REQUIRE_EQ(queue.size(), UsbTransmitQueue::maximum_items);
    REQUIRE(!queue.failed());
}

TEST_CASE(usb_007_capacity_timeout_discards_all_output_and_latches_failure) {
    UsbTransmitQueue queue;
    for (std::size_t index = 0U; index < UsbTransmitQueue::maximum_items; ++index) {
        REQUIRE(queue.enqueue(ByteVector{static_cast<std::uint8_t>(index)}));
    }
    REQUIRE(!queue.enqueue(ByteVector{0xffU}, std::chrono::milliseconds(0)));
    REQUIRE_EQ(queue.size(), 0U);
    REQUIRE(queue.failed());
    REQUIRE(!queue.enqueue(ByteVector{0x01U}));
    queue.reset_failure();
    REQUIRE(queue.enqueue(ByteVector{0x01U}));
}
