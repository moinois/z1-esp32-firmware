// Verifies update phase recovery, publication, reporting, and progress policy.
#include "test.hpp"

#include "firmware/application/update_phase.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using firmware::application::UpdatePhasePort;
using firmware::application::UpdatePhaseService;
using firmware::application::UpdateStatus;

namespace {

// Records phase persistence and broadcast attempts for update-state tests.
class FakeUpdatePhasePort final : public UpdatePhasePort {
public:
    // Records one newest phase persistence request.
    bool persist_phase(std::uint8_t phase) override {
        persisted.push_back(phase);
        return persistence_succeeds;
    }

    // Records one host update-error broadcast.
    void broadcast(std::uint8_t type, std::string_view payload) override {
        broadcast_types.push_back(type);
        broadcasts.emplace_back(payload);
    }

    bool persistence_succeeds = true;
    std::vector<std::uint8_t> persisted;
    std::vector<std::uint8_t> broadcast_types;
    std::vector<std::string> broadcasts;
};

}  // namespace

TEST_CASE(upd_040_boot_reconciliation_handles_only_phases_three_and_four) {
    FakeUpdatePhasePort port;
    UpdatePhaseService phases(port);

    phases.reconcile_boot(1U, 0U);
    phases.reconcile_boot(2U, 10U);
    phases.reconcile_boot(9U, 20U);
    REQUIRE(port.persisted.empty());
    REQUIRE(port.broadcasts.empty());

    phases.reconcile_boot(3U, 30U);
    phases.reconcile_boot(4U, 40U);
    REQUIRE_EQ(port.persisted, std::vector<std::uint8_t>({0U}));
    REQUIRE_EQ(port.broadcast_types, std::vector<std::uint8_t>({0x90U}));
    REQUIRE_EQ(port.broadcasts,
               std::vector<std::string>({
                   "Error: Previous firmware upgrade failed. Please re-upload the firmware package."}));
}

TEST_CASE(upd_042_opening_aggregate_clears_only_persisted_failure_phase) {
    FakeUpdatePhasePort port;
    UpdatePhaseService phases(port);
    phases.reconcile_boot(3U, 0U);
    port.persisted.clear();

    phases.aggregate_opened();
    phases.aggregate_opened();

    REQUIRE_EQ(port.persisted, std::vector<std::uint8_t>({0U}));
}

TEST_CASE(upd_043_publication_changes_volatile_state_even_when_queue_is_full) {
    FakeUpdatePhasePort port;
    UpdatePhaseService phases(port);

    phases.publish(1U);
    phases.publish(2U);
    phases.publish(3U);
    phases.publish(4U);
    phases.publish(5U);

    REQUIRE_EQ(phases.status(), UpdateStatus({5U, 0U}));
    REQUIRE_EQ(phases.pending_count(), 4U);
    phases.process_pending();
    REQUIRE_EQ(port.persisted, std::vector<std::uint8_t>({4U}));
    REQUIRE_EQ(phases.pending_count(), 0U);
}

TEST_CASE(upd_043_processing_keeps_only_newest_and_ignores_persist_failure) {
    FakeUpdatePhasePort port;
    port.persistence_succeeds = false;
    UpdatePhaseService phases(port);

    phases.publish(1U);
    phases.publish(3U);
    phases.process_pending();
    phases.process_pending();

    REQUIRE_EQ(port.persisted, std::vector<std::uint8_t>({3U}));
    REQUIRE_EQ(phases.status(), UpdateStatus({3U, 0U}));
}

TEST_CASE(upd_023_validation_and_previous_failure_share_one_second_limit) {
    FakeUpdatePhasePort port;
    UpdatePhaseService phases(port);

    phases.broadcast_validation_error(100U);
    phases.reconcile_boot(3U, 1099U);
    phases.reconcile_boot(3U, 1100U);

    REQUIRE_EQ(port.broadcasts.size(), 2U);
    REQUIRE_EQ(
        port.broadcasts[0],
        std::string(
            "Error: The firmware file format is incorrect or it has been damaged. Please re-upload"));
    REQUIRE_EQ(
        port.broadcasts[1],
        std::string(
            "Error: Previous firmware upgrade failed. Please re-upload the firmware package."));
}

TEST_CASE(upd_050_and_052_only_persisted_failure_affects_idle_status) {
    FakeUpdatePhasePort port;
    UpdatePhaseService phases(port);

    phases.reconcile_boot(1U, 0U);
    REQUIRE_EQ(phases.status(), UpdateStatus({0U, 0U}));
    phases.reconcile_boot(2U, 1000U);
    REQUIRE_EQ(phases.status(), UpdateStatus({0U, 0U}));
    phases.reconcile_boot(3U, 2000U);
    REQUIRE_EQ(phases.status(), UpdateStatus({3U, 0U}));

    phases.publish(1U);
    REQUIRE_EQ(phases.status(), UpdateStatus({1U, 0U}));
}

TEST_CASE(upd_051_controller_progress_rounds_and_updates_only_in_phase_two) {
    FakeUpdatePhasePort port;
    UpdatePhaseService phases(port);

    phases.set_controller_progress(1U, 3U);
    REQUIRE_EQ(phases.status(), UpdateStatus({0U, 0U}));
    phases.publish(2U);
    phases.set_controller_progress(1U, 3U);
    REQUIRE_EQ(phases.status(), UpdateStatus({2U, 33U}));
    phases.set_controller_progress(4U, 3U);
    REQUIRE_EQ(phases.status(), UpdateStatus({2U, 100U}));
}

TEST_CASE(upd_054_success_displays_for_three_seconds_then_returns_idle) {
    FakeUpdatePhasePort port;
    UpdatePhaseService phases(port);

    phases.controller_completed(5000U);
    REQUIRE_EQ(phases.status(), UpdateStatus({4U, 100U}));
    phases.tick(7999U);
    REQUIRE_EQ(phases.status(), UpdateStatus({4U, 100U}));
    phases.tick(8000U);
    REQUIRE_EQ(phases.status(), UpdateStatus({0U, 0U}));
    phases.process_pending();

    REQUIRE_EQ(port.persisted, std::vector<std::uint8_t>({4U}));
}
