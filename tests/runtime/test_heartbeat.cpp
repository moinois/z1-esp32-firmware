// Tests GPIO-independent heartbeat startup, timing, and failure isolation.
#include "test.hpp"

#include "firmware/application/heartbeat.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

class FakeHeartbeatPort final : public firmware::application::HeartbeatPort {
public:
    bool configure_output() override {
        ++configure_count;
        return configuration_succeeds;
    }
    void set_level(bool high) override { levels.push_back(high); }
    void delay_milliseconds(std::uint32_t duration) override {
        delays.push_back(duration);
    }

    bool configuration_succeeds = true;
    std::size_t configure_count = 0U;
    std::vector<bool> levels;
    std::vector<std::uint32_t> delays;
};

}  // namespace

TEST_CASE(hw_060_heartbeat_starts_high_and_inverts_every_second) {
    FakeHeartbeatPort port;
    firmware::application::HeartbeatService service(port);

    REQUIRE(service.start());
    service.run_cycle();
    service.run_cycle();

    REQUIRE_EQ(port.configure_count, 1U);
    REQUIRE_EQ(port.levels, std::vector<bool>({true, false, true}));
    REQUIRE_EQ(port.delays, std::vector<std::uint32_t>({1000U, 1000U}));
}

TEST_CASE(hw_061_failed_heartbeat_initialization_has_no_followup_side_effects) {
    FakeHeartbeatPort port;
    port.configuration_succeeds = false;
    firmware::application::HeartbeatService service(port);

    REQUIRE(!service.start());
    service.run_cycle();

    REQUIRE_EQ(port.configure_count, 1U);
    REQUIRE(port.levels.empty());
    REQUIRE(port.delays.empty());
}
