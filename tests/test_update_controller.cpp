// Verifies controller-update reset monitoring, failures, and completion.
#include "test.hpp"

#include "firmware/application/update_controller.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using firmware::application::UpdateControllerMonitor;
using firmware::application::UpdateControllerPort;

namespace {

// Records staged-file, transfer-channel, phase, reset, and completion behavior.
class FakeUpdateControllerPort final : public UpdateControllerPort {
public:
    // Reports whether staged controller firmware currently exists.
    bool staged_controller_exists() const override {
        return staged_exists;
    }

    // Reports whether controller firmware transfer is active.
    bool firmware_transfer_active() const override {
        return firmware_active;
    }

    // Reports whether controller configuration transfer is active.
    bool configuration_transfer_active() const override {
        return configuration_active;
    }

    // Reports whether controller factory-data transfer is active.
    bool factory_transfer_active() const override {
        return factory_active;
    }

    // Records one exact controller reset request.
    void send_controller_reset() override {
        calls.emplace_back("reset");
    }

    // Records an update failure phase publication.
    void publish_error() override {
        calls.emplace_back("error");
    }

    // Attempts staged-image deletion and records its exact path.
    void remove_staged_controller(std::string_view path) override {
        calls.emplace_back("remove");
        removed_paths.emplace_back(path);
    }

    // Publishes transient and persisted controller completion.
    void controller_completed(std::uint64_t now_milliseconds) override {
        calls.emplace_back("complete");
        completion_times.push_back(now_milliseconds);
    }

    bool staged_exists = true;
    bool firmware_active = false;
    bool configuration_active = false;
    bool factory_active = false;
    std::vector<std::string> calls;
    std::vector<std::string> removed_paths;
    std::vector<std::uint64_t> completion_times;
};

}  // namespace

TEST_CASE(upd_055_monitor_checks_immediately_then_every_five_seconds) {
    FakeUpdateControllerPort port;
    UpdateControllerMonitor monitor(port);
    monitor.start(100U);

    monitor.tick(100U);
    monitor.tick(5099U);
    monitor.tick(5100U);
    monitor.tick(12100U);

    REQUIRE_EQ(port.calls,
               std::vector<std::string>({"reset", "reset", "reset"}));
}

TEST_CASE(upd_055_any_transfer_channel_suppresses_reset_until_later_check) {
    FakeUpdateControllerPort port;
    UpdateControllerMonitor monitor(port);
    monitor.start(0U);

    port.firmware_active = true;
    monitor.tick(0U);
    port.firmware_active = false;
    port.configuration_active = true;
    monitor.tick(5000U);
    port.configuration_active = false;
    port.factory_active = true;
    monitor.tick(10000U);
    port.factory_active = false;
    monitor.tick(15000U);

    REQUIRE_EQ(port.calls, std::vector<std::string>({"reset"}));
}

TEST_CASE(upd_055_missing_staged_content_suppresses_periodic_reset) {
    FakeUpdateControllerPort port;
    port.staged_exists = false;
    UpdateControllerMonitor monitor(port);
    monitor.start(0U);

    monitor.tick(0U);
    monitor.tick(5000U);

    REQUIRE(port.calls.empty());
}

TEST_CASE(upd_053_transfer_failures_change_phase_only_when_content_exists) {
    FakeUpdateControllerPort port;
    UpdateControllerMonitor monitor(port);

    monitor.transfer_failed();
    monitor.transfer_cancelled();
    monitor.transfer_timed_out(true);
    monitor.transfer_timed_out(false);
    REQUIRE_EQ(port.calls,
               std::vector<std::string>({"error", "error", "error"}));

    port.staged_exists = false;
    monitor.transfer_failed();
    monitor.transfer_cancelled();
    monitor.transfer_timed_out(true);
    REQUIRE_EQ(port.calls.size(), 3U);
}

TEST_CASE(upd_054_controller_completion_deletes_then_publishes_regardless) {
    FakeUpdateControllerPort port;
    UpdateControllerMonitor monitor(port);

    monitor.controller_completed(7000U);

    REQUIRE_EQ(port.calls,
               std::vector<std::string>({"remove", "complete"}));
    REQUIRE_EQ(port.removed_paths,
               std::vector<std::string>({"/sd/lpc1768.bin"}));
    REQUIRE_EQ(port.completion_times,
               std::vector<std::uint64_t>({7000U}));
}
