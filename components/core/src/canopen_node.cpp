// Implements target-independent CANopen NMT and producer-heartbeat timing.
#include "firmware/core/canopen_node.hpp"

#include <algorithm>
#include <cstdint>

namespace firmware::core {
namespace {

constexpr std::uint8_t nmt_start = 0x01U;
constexpr std::uint8_t nmt_stop = 0x02U;
constexpr std::uint8_t nmt_pre_operational = 0x80U;
constexpr std::uint8_t nmt_reset_node = 0x81U;
constexpr std::uint8_t nmt_reset_communication = 0x82U;
constexpr std::uint8_t generic_or_communication_error_mask = 0x11U;
constexpr std::uint8_t nmt_frame_size = 2U;
constexpr std::uint8_t heartbeat_frame_size = 1U;
constexpr std::uint8_t broadcast_node_id = 0U;

// Decrements one bounded millisecond counter by the fixed processing period.
void decrement_cycle(std::uint16_t& remaining_milliseconds) {
    if (remaining_milliseconds <= canopen::processing_period_milliseconds) {
        remaining_milliseconds = 0U;
        return;
    }
    remaining_milliseconds -= canopen::processing_period_milliseconds;
}

}  // namespace

void CanopenNode::accept_nmt(const CanFrame& frame) {
    if (frame.identifier != canopen::nmt_identifier ||
        frame.size != nmt_frame_size) {
        return;
    }
    const std::uint8_t target = frame.data[1];
    if (target != canopen::node_id && target != broadcast_node_id) {
        return;
    }

    switch (frame.data[0]) {
        case nmt_start:
            select_state(NmtState::operational);
            break;
        case nmt_stop:
            select_state(NmtState::stopped);
            break;
        case nmt_pre_operational:
            select_state(NmtState::pre_operational);
            break;
        case nmt_reset_node:
            restart_remaining_milliseconds_ =
                canopen::restart_delay_milliseconds;
            break;
        case nmt_reset_communication:
            reset_communication();
            break;
        default:
            break;
    }
}

void CanopenNode::set_producer_heartbeat_period(
    std::uint16_t period_milliseconds) {
    producer_period_milliseconds_ = period_milliseconds;
    heartbeat_remaining_milliseconds_ = period_milliseconds;
    heartbeat_forced_ = period_milliseconds != 0U;
}

void CanopenNode::set_error_register(std::uint8_t error_register) {
    if (state_ == NmtState::operational &&
        (error_register & generic_or_communication_error_mask) != 0U) {
        select_state(NmtState::pre_operational);
    }
}

CanopenCycleResult CanopenNode::process_cycle() {
    CanopenCycleResult result;

    if (restart_remaining_milliseconds_ != 0U) {
        decrement_cycle(restart_remaining_milliseconds_);
        if (restart_remaining_milliseconds_ == 0U) {
            result.restart_mainboard = true;
        }
    }

    if (bootup_pending_) {
        result.frame = heartbeat_frame(NmtState::initializing);
        state_ = NmtState::operational;
        bootup_pending_ = false;
        if (producer_period_milliseconds_ != 0U) {
            decrement_cycle(heartbeat_remaining_milliseconds_);
        }
        return result;
    }

    if (producer_period_milliseconds_ == 0U) {
        return result;
    }
    if (heartbeat_forced_) {
        result.frame = heartbeat_frame(state_);
        heartbeat_forced_ = false;
        heartbeat_remaining_milliseconds_ = producer_period_milliseconds_;
        return result;
    }

    decrement_cycle(heartbeat_remaining_milliseconds_);
    if (heartbeat_remaining_milliseconds_ == 0U) {
        result.frame = heartbeat_frame(state_);
        heartbeat_remaining_milliseconds_ = producer_period_milliseconds_;
    }
    return result;
}

NmtState CanopenNode::state() const {
    return state_;
}

std::uint16_t CanopenNode::producer_heartbeat_period() const {
    return producer_period_milliseconds_;
}

void CanopenNode::reset_communication() {
    state_ = NmtState::initializing;
    bootup_pending_ = true;
    heartbeat_forced_ = false;
    heartbeat_remaining_milliseconds_ = std::min<std::uint16_t>(
        producer_period_milliseconds_,
        canopen::first_heartbeat_limit_milliseconds);
}

void CanopenNode::select_state(NmtState state) {
    if (state_ == state) {
        return;
    }
    state_ = state;
    if (producer_period_milliseconds_ != 0U) {
        heartbeat_forced_ = true;
    }
}

CanFrame CanopenNode::heartbeat_frame(NmtState state) {
    CanFrame frame;
    frame.identifier = canopen::heartbeat_identifier;
    frame.size = heartbeat_frame_size;
    frame.data[0] = static_cast<std::uint8_t>(state);
    return frame;
}

}  // namespace firmware::core
