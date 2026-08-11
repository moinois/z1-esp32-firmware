// Verifies USB enumeration, protocol activation, and disconnect behavior.
#include "test.hpp"

#include "application/usb/usb_protocol_state.hpp"

#include <chrono>

using firmware::application::UsbProtocolState;
using firmware::core::ByteVector;

TEST_CASE(usb_004_enumeration_does_not_activate_protocol_until_valid_frame) {
    UsbProtocolState state;
    state.enumerated();
    REQUIRE(state.physically_present());
    REQUIRE(!state.protocol_active());
    REQUIRE(!state.can_send());
    state.valid_frame_received();
    REQUIRE(state.protocol_active());
    REQUIRE(state.can_send());
}

TEST_CASE(usb_005_and_usb_008_disconnect_clears_activity_receive_and_output) {
    UsbProtocolState state;
    state.enumerated();
    state.receive_staging().stage(ByteVector{0x86U});
    state.transmit_queue().enqueue(ByteVector{0x01U});
    state.valid_frame_received();
    state.disconnected();
    REQUIRE(!state.physically_present());
    REQUIRE(!state.protocol_active());
    REQUIRE(!state.can_send());
    REQUIRE_EQ(state.receive_staging().size(), 0U);
    REQUIRE_EQ(state.transmit_queue().size(), 0U);
}

TEST_CASE(usb_007_queue_failure_disables_destination_until_reenumeration) {
    UsbProtocolState state;
    state.enumerated();
    state.valid_frame_received();
    for (std::size_t index = 0U;
         index < firmware::application::UsbTransmitQueue::maximum_items; ++index) {
        REQUIRE(state.transmit_queue().enqueue(ByteVector{0x01U}));
    }
    REQUIRE(!state.transmit_queue().enqueue(
        ByteVector{0x02U}, std::chrono::milliseconds(0)));
    REQUIRE(!state.can_send());
    state.disconnected();
    state.enumerated();
    state.valid_frame_received();
    REQUIRE(state.can_send());
}

TEST_CASE(usb_009_invalid_frame_before_enumeration_cannot_activate_protocol) {
    UsbProtocolState state;
    state.valid_frame_received();
    REQUIRE(!state.protocol_active());
    REQUIRE(!state.can_send());
}
