// Verifies USB enumeration, protocol activation, and disconnect behavior.
#include "test.hpp"

#include "firmware/application/usb_protocol_state.hpp"

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

TEST_CASE(usb_005_disconnect_clears_activity_and_receive_but_keeps_output) {
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
    REQUIRE_EQ(state.transmit_queue().size(), 1U);
}

TEST_CASE(usb_009_invalid_frame_before_enumeration_cannot_activate_protocol) {
    UsbProtocolState state;
    state.valid_frame_received();
    REQUIRE(!state.protocol_active());
    REQUIRE(!state.can_send());
}

