// Verifies live-video command matching, ownership, and preemption policy.
#include "test.hpp"

#include "application/runtime/live_control_policy.hpp"

#include <string_view>
#include <vector>

using firmware::application::LiveControlAction;
using firmware::application::LiveControlDecision;
using firmware::application::LiveControlPolicy;

TEST_CASE(live_001_exact_commands_start_stop_and_ignore_other_payloads) {
    LiveControlPolicy policy;
    REQUIRE_EQ(policy.handle(7U, "other"), std::vector<LiveControlDecision>{});
    REQUIRE_EQ(policy.handle(7U, "start_stream"),
               (std::vector<LiveControlDecision>{{LiveControlAction::start, 7U}}));
    REQUIRE_EQ(policy.handle(7U, "stop_stream"),
               (std::vector<LiveControlDecision>{{LiveControlAction::stop, 7U}}));
    REQUIRE_EQ(policy.handle(7U, "stop_stream"), std::vector<LiveControlDecision>{});
}

TEST_CASE(live_005_and_006_new_owner_stops_old_stream_and_notifies_old_socket) {
    LiveControlPolicy policy;
    REQUIRE_EQ(policy.handle(1U, "start_stream"),
               (std::vector<LiveControlDecision>{{LiveControlAction::start, 1U}}));
    REQUIRE_EQ(policy.handle(2U, "start_stream"),
               std::vector<LiveControlDecision>(
                   {{LiveControlAction::stop, 1U},
                    {LiveControlAction::preempted, 1U},
                    {LiveControlAction::start, 2U}}));
    REQUIRE_EQ(policy.handle(1U, "stop_stream"), std::vector<LiveControlDecision>{});
}

TEST_CASE(live_006_same_socket_restart_does_not_preempt_itself) {
    LiveControlPolicy policy;
    policy.handle(4U, "start_stream");
    REQUIRE_EQ(policy.handle(4U, "start_stream"),
               std::vector<LiveControlDecision>{});
    REQUIRE_EQ(policy.handle(4U, "stop_stream"),
               (std::vector<LiveControlDecision>{{LiveControlAction::stop, 4U}}));
}

TEST_CASE(live_008_disconnect_stops_only_the_owning_socket) {
    LiveControlPolicy policy;
    policy.handle(9U, "start_stream");
    REQUIRE_EQ(policy.on_disconnect(8U), std::vector<LiveControlDecision>{});
    REQUIRE_EQ(policy.on_disconnect(9U),
               (std::vector<LiveControlDecision>{{LiveControlAction::stop, 9U}}));
}
