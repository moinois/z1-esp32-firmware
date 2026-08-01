// Verifies the production USB drain through an instrumented endpoint port.
#include "test.hpp"

#include "firmware/application/usb_transmit_drain.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

using firmware::application::UsbTransmitDrain;
using firmware::application::UsbTransmitDrainPort;
using firmware::application::UsbTransmitQueue;
using firmware::core::ByteVector;
using firmware::core::BytesView;

namespace {

class FakeEndpoint final : public UsbTransmitDrainPort {
public:
    std::size_t available() override { return capacity; }

    std::size_t write(BytesView bytes) override {
        const std::size_t count = std::min(bytes.size(), write_limit);
        written.insert(written.end(), bytes.begin(), bytes.begin() + count);
        return count;
    }

    void flush() override { ++flush_count; }

    std::size_t capacity = 64U;
    std::size_t write_limit = 64U;
    ByteVector written;
    std::size_t flush_count = 0U;
};

}  // namespace

TEST_CASE(usb_007_drain_continues_partial_writes_and_pops_only_after_completion) {
    UsbTransmitQueue queue;
    FakeEndpoint endpoint;
    endpoint.capacity = 3U;
    endpoint.write_limit = 2U;
    REQUIRE(queue.enqueue(ByteVector{1U, 2U, 3U, 4U, 5U}));
    UsbTransmitDrain drain(queue, endpoint);

    drain.process(true, 100U);
    REQUIRE_EQ(queue.size(), 1U);
    drain.process(true, 200U);
    REQUIRE_EQ(queue.size(), 1U);
    drain.process(true, 300U);

    REQUIRE_EQ(endpoint.written, ByteVector({1U, 2U, 3U, 4U, 5U}));
    REQUIRE_EQ(endpoint.flush_count, 4U);
    REQUIRE_EQ(queue.size(), 0U);
}

TEST_CASE(usb_008_drain_discards_one_stalled_frame_after_exact_timeout) {
    UsbTransmitQueue queue;
    FakeEndpoint endpoint;
    endpoint.capacity = 0U;
    REQUIRE(queue.enqueue(ByteVector{1U}));
    REQUIRE(queue.enqueue(ByteVector{2U}));
    UsbTransmitDrain drain(queue, endpoint);

    drain.process(true, 1000U);
    drain.process(true, 1500U);
    REQUIRE_EQ(queue.size(), 2U);
    drain.process(true, 1501U);
    REQUIRE_EQ(queue.size(), 1U);

    endpoint.capacity = 1U;
    drain.process(true, 1502U);
    REQUIRE_EQ(endpoint.written, ByteVector({2U}));
    REQUIRE_EQ(queue.size(), 0U);
}

TEST_CASE(usb_005_disconnect_retains_queue_and_restarts_frame_from_its_boundary) {
    UsbTransmitQueue queue;
    FakeEndpoint endpoint;
    endpoint.capacity = 2U;
    REQUIRE(queue.enqueue(ByteVector{1U, 2U, 3U, 4U}));
    UsbTransmitDrain drain(queue, endpoint);

    drain.process(true, 100U);
    REQUIRE_EQ(endpoint.written, ByteVector({1U, 2U}));
    drain.process(false, 200U);
    REQUIRE_EQ(queue.size(), 1U);
    endpoint.written.clear();
    endpoint.capacity = 4U;
    drain.process(true, 300U);

    REQUIRE_EQ(endpoint.written, ByteVector({1U, 2U, 3U, 4U}));
    REQUIRE_EQ(queue.size(), 0U);
}
