// Verifies TCP transmit bounds, FIFO ordering, and whole-frame removal.
#include "test.hpp"
#include "application/transport/tcp_transmit_queue.hpp"

TEST_CASE(tcp_006_queue_rejects_empty_oversized_and_thirty_third_frame) {
    firmware::application::TcpTransmitQueue queue;
    REQUIRE(!queue.enqueue(firmware::core::BytesView()));
    const std::string oversized(8301U, 'x');
    REQUIRE(!queue.enqueue(firmware::core::BytesView(oversized)));
    const std::string frame("a");
    for (int index = 0; index < 32; ++index) {
        REQUIRE(queue.enqueue(firmware::core::BytesView(frame)));
    }
    REQUIRE(!queue.enqueue(firmware::core::BytesView(frame)));
    REQUIRE_EQ(queue.size(), 32U);
}

TEST_CASE(tcp_007_queue_preserves_front_until_transport_completion) {
    firmware::application::TcpTransmitQueue queue;
    REQUIRE(queue.enqueue(firmware::core::BytesView("first")));
    REQUIRE(queue.enqueue(firmware::core::BytesView("second")));
    REQUIRE_EQ(*queue.front(), firmware::core::ByteVector({'f', 'i', 'r', 's', 't'}));
    queue.pop_front();
    REQUIRE_EQ(*queue.front(), firmware::core::ByteVector({'s', 'e', 'c', 'o', 'n', 'd'}));
}
