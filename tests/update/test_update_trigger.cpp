// Verifies aggregate update boot ordering, command prefixes, and coalescing.
#include "test.hpp"

#include "application/update/update_trigger.hpp"
#include "application/update/update_task_initialization.hpp"

#include <string>
#include <string_view>
#include <vector>

using firmware::application::UpdateTriggerPort;
using firmware::application::UpdateTriggerService;
using firmware::application::UpdateTaskInitialization;
using firmware::application::UpdateTaskInitializationPort;
using firmware::application::UpdateMonitorInitialization;
using firmware::application::UpdateMonitorInitializationPort;

namespace {

// Records partial cleanup and persisted-state reconciliation in call order.
class FakeUpdateTriggerPort final : public UpdateTriggerPort {
public:
    // Records best-effort partial aggregate cleanup.
    void remove_partial(std::string_view path) override {
        calls.emplace_back("remove");
        paths.emplace_back(path);
    }

    // Records persisted update-state reconciliation.
    void reconcile_persisted_state() override {
        calls.emplace_back("reconcile");
    }

    std::vector<std::string> calls;
    std::vector<std::string> paths;
};

class FakeUpdateTaskInitializationPort final
    : public UpdateTaskInitializationPort {
public:
    bool start_processing() override {
        calls.emplace_back("start");
        const bool result = start_results.at(start_index);
        ++start_index;
        return result;
    }
    void warn_not_started() override { calls.emplace_back("warn"); }
    void processing_started() override { calls.emplace_back("started"); }
    void processing_start_failed() override { calls.emplace_back("failed"); }
    void trigger_processing() override { calls.emplace_back("trigger"); }

    std::vector<bool> start_results;
    std::size_t start_index = 0U;
    std::vector<std::string> calls;
};

class FakeUpdateMonitorInitializationPort final
    : public UpdateMonitorInitializationPort {
public:
    void start_monitor() override { ++start_count; }

    std::size_t start_count = 0U;
};

}  // namespace

TEST_CASE(upd_003_boot_cleans_reconciles_then_requests_processing) {
    FakeUpdateTriggerPort port;
    UpdateTriggerService trigger(port);

    trigger.boot();

    REQUIRE_EQ(port.calls,
               std::vector<std::string>({"remove", "reconcile"}));
    REQUIRE_EQ(port.paths,
               std::vector<std::string>({"/sd/firmware.bin.part"}));
    REQUIRE(trigger.take_request());
    REQUIRE(!trigger.take_request());
}

TEST_CASE(upd_002_upgrade_and_reset_prefixes_request_without_shape_validation) {
    FakeUpdateTriggerPort port;
    UpdateTriggerService trigger(port);

    REQUIRE(trigger.handle_command("upgrade anything"));
    REQUIRE(trigger.take_request());
    REQUIRE(trigger.handle_command("reset-now"));
    REQUIRE(trigger.take_request());
}

TEST_CASE(upd_002_update_trigger_prefixes_are_case_sensitive) {
    FakeUpdateTriggerPort port;
    UpdateTriggerService trigger(port);

    REQUIRE(!trigger.handle_command("Upgrade"));
    REQUIRE(!trigger.handle_command(" reset"));
    REQUIRE(!trigger.handle_command("restart"));
    REQUIRE(!trigger.take_request());
}

TEST_CASE(upd_002_multiple_pending_requests_coalesce_into_one_operation) {
    FakeUpdateTriggerPort port;
    UpdateTriggerService trigger(port);

    trigger.boot();
    REQUIRE(trigger.handle_command("upgrade"));
    REQUIRE(trigger.handle_command("reset"));

    REQUIRE(trigger.take_request());
    REQUIRE(!trigger.take_request());
}

TEST_CASE(upd_006_boot_initializes_once_then_submits_the_boot_request) {
    FakeUpdateTaskInitializationPort port;
    port.start_results = {true};
    UpdateTaskInitialization initialization(port);

    initialization.boot();

    REQUIRE_EQ(port.calls,
               std::vector<std::string>({"start", "started", "trigger"}));
}

TEST_CASE(upd_006_boot_request_retries_one_failed_direct_initialization) {
    FakeUpdateTaskInitializationPort port;
    port.start_results = {false, true};
    UpdateTaskInitialization initialization(port);

    initialization.boot();

    REQUIRE_EQ(port.calls, std::vector<std::string>(
                               {"start", "failed", "warn", "start", "started", "trigger"}));
}

TEST_CASE(upd_006_unavailable_requests_are_dropped_after_a_failed_retry) {
    FakeUpdateTaskInitializationPort port;
    port.start_results = {false, false, false, true};
    UpdateTaskInitialization initialization(port);

    initialization.boot();
    initialization.request();
    initialization.request();

    REQUIRE_EQ(port.calls,
               std::vector<std::string>({"start", "failed", "warn", "start",
                                         "failed", "warn", "start", "failed",
                                         "warn", "start", "started", "trigger"}));
}

TEST_CASE(upd_006_available_later_requests_do_not_reinitialize) {
    FakeUpdateTaskInitializationPort port;
    port.start_results = {true};
    UpdateTaskInitialization initialization(port);

    initialization.boot();
    initialization.request();
    initialization.request();

    REQUIRE_EQ(port.calls, std::vector<std::string>(
                               {"start", "started", "trigger", "trigger", "trigger"}));
}

TEST_CASE(upd_055_monitor_has_exactly_one_startup_attempt) {
    FakeUpdateMonitorInitializationPort port;
    UpdateMonitorInitialization initialization(port);

    initialization.start();
    initialization.start();
    initialization.start();

    REQUIRE_EQ(port.start_count, 1U);
}
