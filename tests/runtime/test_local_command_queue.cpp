// Verifies bounded FIFO admission and ordering for local command frames.
#include "test.hpp"

#include "application/runtime/local_command_queue.hpp"

using firmware::application::LocalCommandQueue;
using firmware::core::Frame;

TEST_CASE(cmd_004_local_queue_is_fifo_and_bounded_to_32_commands) {
    LocalCommandQueue queue;
    for (std::uint8_t type = 0U; type < 32U; ++type) {
        REQUIRE(queue.enqueue(Frame{type, {'x'}}));
    }
    REQUIRE(!queue.enqueue(Frame{0xffU, {'x'}}));
    REQUIRE_EQ(queue.pending(), 32U);

    for (std::uint8_t type = 0U; type < 32U; ++type) {
        const auto command = queue.dequeue();
        REQUIRE(command.has_value());
        REQUIRE_EQ(command->type, type);
    }
    REQUIRE(!queue.dequeue().has_value());
}

TEST_CASE(cmd_004_local_queue_rejects_empty_and_oversized_frames) {
    LocalCommandQueue queue;
    REQUIRE(queue.enqueue(Frame{0x90U, {}}));
    const std::size_t payload_size = LocalCommandQueue::maximum_frame_size - 9U;
    REQUIRE(queue.enqueue(Frame{0x90U, firmware::core::ByteVector(payload_size, 'a')}));
    REQUIRE(!queue.enqueue(Frame{0x90U,
                                 firmware::core::ByteVector(payload_size + 1U, 'a')}));
}
