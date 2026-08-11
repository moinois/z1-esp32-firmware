/** @file @brief Implements sticky, request-triggered live-media initialization. */
#include "application/runtime/live_initialization.hpp"

namespace firmware::application {

LiveInitialization::LiveInitialization(LiveInitializationPort& port)
    : port_(port) {}

bool LiveInitialization::ensure_available() {
    State state = state_.load(std::memory_order_acquire);
    if (state == State::available) {
        return true;
    }
    if (state != State::not_attempted) {
        return false;
    }
    State expected = State::not_attempted;
    if (!state_.compare_exchange_strong(expected, State::initializing,
                                        std::memory_order_acq_rel)) {
        return expected == State::available;
    }
    const bool available = port_.initialize_live_media();
    state_.store(available ? State::available : State::unavailable,
                 std::memory_order_release);
    return available;
}

}  // namespace firmware::application
