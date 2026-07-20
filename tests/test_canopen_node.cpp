// Tests deterministic CANopen identity, NMT, heartbeat, and error transitions.
#include "test.hpp"

#include "firmware/core/canopen_node.hpp"

using firmware::core::CanFrame;
using firmware::core::CanopenNode;
using firmware::core::NmtState;

namespace {

// Creates an exact two-byte NMT request for the local node by default.
CanFrame nmt(std::uint8_t command,
             std::uint8_t target = firmware::core::canopen::node_id) {
    CanFrame frame;
    frame.identifier = firmware::core::canopen::nmt_identifier;
    frame.size = 2U;
    frame.data[0] = command;
    frame.data[1] = target;
    return frame;
}

}  // namespace

TEST_CASE(can_002_and_003_first_cycle_sends_bootup_then_enters_operational) {
    CanopenNode node;

    REQUIRE_EQ(node.state(), NmtState::initializing);
    const auto first = node.process_cycle();

    REQUIRE(first.frame.has_value());
    REQUIRE_EQ(first.frame->identifier, 0x711U);
    REQUIRE_EQ(first.frame->size, 1U);
    REQUIRE_EQ(first.frame->data[0], 0U);
    REQUIRE_EQ(node.state(), NmtState::operational);
    REQUIRE(!node.process_cycle().frame.has_value());
    REQUIRE_EQ(firmware::core::canopen::sdo_request_identifier, 0x611U);
    REQUIRE_EQ(firmware::core::canopen::sdo_response_identifier, 0x591U);
}

TEST_CASE(can_002_nonzero_heartbeat_write_publishes_on_next_cycle_then_periodically) {
    CanopenNode node;
    node.process_cycle();

    node.set_producer_heartbeat_period(30U);
    const auto immediate = node.process_cycle();
    REQUIRE_EQ(immediate.frame->data[0], 5U);

    REQUIRE(!node.process_cycle().frame.has_value());
    REQUIRE(!node.process_cycle().frame.has_value());
    const auto periodic = node.process_cycle();
    REQUIRE(periodic.frame.has_value());
    REQUIRE_EQ(periodic.frame->data[0], 5U);
}

TEST_CASE(can_002_communication_reset_retains_period_and_shortens_first_timer) {
    CanopenNode node;
    node.process_cycle();
    node.set_producer_heartbeat_period(20U);
    node.process_cycle();

    node.accept_nmt(nmt(0x82U));
    const auto bootup = node.process_cycle();
    REQUIRE_EQ(bootup.frame->data[0], 0U);
    const auto first_heartbeat = node.process_cycle();
    REQUIRE_EQ(first_heartbeat.frame->data[0], 5U);
}

TEST_CASE(can_002_communication_reset_caps_first_heartbeat_timer_at_500_ms) {
    CanopenNode node;
    node.process_cycle();
    node.set_producer_heartbeat_period(1000U);
    node.process_cycle();
    node.accept_nmt(nmt(0x82U));
    node.process_cycle();

    for (std::size_t cycle = 0U; cycle < 48U; ++cycle) {
        REQUIRE(!node.process_cycle().frame.has_value());
    }
    REQUIRE(node.process_cycle().frame.has_value());
}

TEST_CASE(can_005_nmt_targets_and_state_commands_follow_protocol) {
    CanopenNode node;
    node.process_cycle();
    node.set_producer_heartbeat_period(100U);
    node.process_cycle();

    node.accept_nmt(nmt(0x02U, 0U));
    REQUIRE_EQ(node.state(), NmtState::stopped);
    REQUIRE_EQ(node.process_cycle().frame->data[0], 4U);

    node.accept_nmt(nmt(0x80U));
    REQUIRE_EQ(node.state(), NmtState::pre_operational);
    REQUIRE_EQ(node.process_cycle().frame->data[0], 127U);

    node.accept_nmt(nmt(0x01U));
    REQUIRE_EQ(node.state(), NmtState::operational);
    REQUIRE_EQ(node.process_cycle().frame->data[0], 5U);

    node.accept_nmt(nmt(0x02U, 0x22U));
    REQUIRE_EQ(node.state(), NmtState::operational);
}

TEST_CASE(can_005_malformed_and_unknown_nmt_frames_have_no_effect) {
    CanopenNode node;
    node.process_cycle();

    CanFrame wrong_identifier = nmt(0x02U);
    wrong_identifier.identifier = 1U;
    node.accept_nmt(wrong_identifier);
    CanFrame wrong_size = nmt(0x02U);
    wrong_size.size = 1U;
    node.accept_nmt(wrong_size);
    node.accept_nmt(nmt(0x7fU));

    REQUIRE_EQ(node.state(), NmtState::operational);
}

TEST_CASE(can_005_reset_node_requests_restart_after_exactly_100_ms) {
    CanopenNode node;
    node.process_cycle();
    node.accept_nmt(nmt(0x81U));

    for (std::size_t cycle = 0U; cycle < 9U; ++cycle) {
        REQUIRE(!node.process_cycle().restart_mainboard);
    }
    REQUIRE(node.process_cycle().restart_mainboard);
    REQUIRE(!node.process_cycle().restart_mainboard);
}

TEST_CASE(can_006_observed_error_bits_leave_operational_state_only) {
    CanopenNode node;
    node.process_cycle();

    node.set_error_register(0x02U);
    REQUIRE_EQ(node.state(), NmtState::operational);
    node.set_error_register(0x10U);
    REQUIRE_EQ(node.state(), NmtState::pre_operational);
    node.set_error_register(0U);
    REQUIRE_EQ(node.state(), NmtState::pre_operational);
}
