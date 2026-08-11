/** @file @brief Implements CAN-007 and CAN-008 pending-send behavior. */
#include "application/can/can_pending_transmitter.hpp"

namespace firmware::application {
namespace {

constexpr std::uint32_t transmit_timeout_milliseconds = 1000U;
constexpr std::uint32_t post_attempt_delay_milliseconds = 10U;

}  // namespace

CanPendingTransmitter::CanPendingTransmitter(CanPendingTransmitPort& port)
    : port_(port) {}

void CanPendingTransmitter::offer(const core::CanFrame& frame) {
    pending_.push_back(frame);
    while (!pending_.empty()) {
        const bool transmitted =
            port_.attempt(pending_.front(), transmit_timeout_milliseconds);
        port_.delay(post_attempt_delay_milliseconds);
        if (!transmitted) return;
        pending_.pop_front();
    }
}

std::size_t CanPendingTransmitter::pending() const {
    return pending_.size();
}

}  // namespace firmware::application
