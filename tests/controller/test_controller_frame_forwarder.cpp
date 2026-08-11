// Verifies controller forwarding preserves framing and output spacing policy.
#include "test.hpp"
#include "application/controller/controller_frame_forwarder.hpp"

TEST_CASE(controller_forwarder_encodes_and_queues_frames) {
    firmware::application::ControllerFrameForwarder forwarder;
    const firmware::core::Frame frame{0x44U, {'o', 'k'}};
    REQUIRE(forwarder.forward(frame));
    REQUIRE_EQ(forwarder.pending(), 1U);
    const auto item = forwarder.take_ready(0U);
    REQUIRE(item.has_value());
    REQUIRE_EQ(*item, firmware::core::encode_frame(frame));
}

TEST_CASE(controller_forwarder_enforces_spacing_between_writes) {
    firmware::application::ControllerFrameForwarder forwarder;
    REQUIRE(forwarder.forward({0x01U, {'a'}}));
    REQUIRE(forwarder.forward({0x02U, {'b'}}));
    REQUIRE(forwarder.take_ready(0U).has_value());
    REQUIRE(!forwarder.take_ready(9U).has_value());
    REQUIRE(forwarder.take_ready(10U).has_value());
}

TEST_CASE(diag_038_controller_forwarder_exposes_full_capacity) {
    firmware::application::ControllerFrameForwarder forwarder;
    for (std::uint8_t index = 0U; index < 32U; ++index) {
        REQUIRE(!forwarder.full());
        REQUIRE(forwarder.forward({0xA2U, {index}}));
    }
    REQUIRE(forwarder.full());
    REQUIRE(!forwarder.forward({0xA2U, {32U}}));
}
