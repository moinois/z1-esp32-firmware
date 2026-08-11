// Verifies pending CAN retry order, timeout, and post-attempt pacing.
#include "test.hpp"

#include "application/can/can_pending_transmitter.hpp"

#include <deque>
#include <vector>

using firmware::application::CanPendingTransmitPort;
using firmware::application::CanPendingTransmitter;
using firmware::core::CanFrame;

namespace {

CanFrame frame(std::uint16_t identifier) {
    CanFrame value;
    value.identifier = identifier;
    return value;
}

class FakeTransmitPort final : public CanPendingTransmitPort {
public:
    bool attempt(const CanFrame& value, std::uint32_t timeout) override {
        attempts.push_back(value.identifier);
        timeouts.push_back(timeout);
        const bool result = outcomes.empty() ? true : outcomes.front();
        if (!outcomes.empty()) outcomes.pop_front();
        return result;
    }
    void delay(std::uint32_t milliseconds) override {
        delays.push_back(milliseconds);
    }

    std::deque<bool> outcomes;
    std::vector<std::uint16_t> attempts;
    std::vector<std::uint32_t> timeouts;
    std::vector<std::uint32_t> delays;
};

}  // namespace

TEST_CASE(can_007_each_attempt_waits_one_second_then_delays_ten_milliseconds) {
    FakeTransmitPort port;
    CanPendingTransmitter transmitter(port);

    transmitter.offer(frame(0x111U));

    REQUIRE_EQ(port.timeouts, std::vector<std::uint32_t>({1000U}));
    REQUIRE_EQ(port.delays, std::vector<std::uint32_t>({10U}));
    REQUIRE_EQ(transmitter.pending(), 0U);
}

TEST_CASE(can_008_failure_stays_pending_and_is_retried_before_new_message) {
    FakeTransmitPort port;
    port.outcomes = {false, true, true};
    CanPendingTransmitter transmitter(port);

    transmitter.offer(frame(0x111U));
    REQUIRE_EQ(transmitter.pending(), 1U);
    transmitter.offer(frame(0x222U));

    REQUIRE_EQ(port.attempts,
               std::vector<std::uint16_t>({0x111U, 0x111U, 0x222U}));
    REQUIRE_EQ(port.delays,
               std::vector<std::uint32_t>({10U, 10U, 10U}));
    REQUIRE_EQ(transmitter.pending(), 0U);
}
