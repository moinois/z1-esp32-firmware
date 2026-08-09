// Verifies aggregate update boot ordering, command prefixes, and coalescing.
#include "test.hpp"

#include "application/update/update_trigger.hpp"

#include <string>
#include <string_view>
#include <vector>

using firmware::application::UpdateTriggerPort;
using firmware::application::UpdateTriggerService;

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
