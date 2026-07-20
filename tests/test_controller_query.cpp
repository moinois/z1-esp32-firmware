// Tests the independent periodic controller query schedules.
#include "test.hpp"

#include "firmware/application/controller_query.hpp"

using firmware::application::ControllerQueryScheduler;

TEST_CASE(lpc_001_both_controller_queries_are_due_immediately) {
    ControllerQueryScheduler scheduler(1000U);

    const auto queries = scheduler.poll(1000U, true);

    REQUIRE_EQ(queries.size(), 2U);
    REQUIRE_EQ(queries[0].type, 0xA1U);
    REQUIRE_EQ(queries[0].payload, firmware::core::ByteVector({'?'}));
    REQUIRE_EQ(queries[1].type, 0xA2U);
    REQUIRE_EQ(queries[1].payload,
               firmware::core::ByteVector({'d', 'i', 'a', 'g', 'n', 'o', 's', 'e', 0}));
}

TEST_CASE(lpc_001_query_periods_are_independent) {
    ControllerQueryScheduler scheduler(0U);
    scheduler.poll(0U, true);

    REQUIRE(scheduler.poll(299U, true).empty());
    REQUIRE_EQ(scheduler.poll(300U, true).front().type, 0xA1U);
    REQUIRE_EQ(scheduler.poll(500U, true).front().type, 0xA2U);
    REQUIRE_EQ(scheduler.poll(600U, true).front().type, 0xA1U);
}

TEST_CASE(lpc_002_suppressed_opportunities_are_skipped_not_deferred) {
    ControllerQueryScheduler scheduler(0U);
    scheduler.poll(0U, true);

    REQUIRE(scheduler.poll(300U, false).empty());
    REQUIRE(scheduler.poll(301U, true).empty());
    REQUIRE_EQ(scheduler.poll(600U, true).front().type, 0xA1U);
}

TEST_CASE(lpc_002_late_poll_advances_deadlines_from_original_schedule) {
    ControllerQueryScheduler scheduler(0U);
    scheduler.poll(0U, true);

    const auto queries = scheduler.poll(1250U, true);

    REQUIRE_EQ(queries.size(), 2U);
    REQUIRE_EQ(queries[0].type, 0xA1U);
    REQUIRE_EQ(queries[1].type, 0xA2U);
    REQUIRE(scheduler.poll(1499U, true).empty());
    REQUIRE_EQ(scheduler.poll(1500U, true).size(), 2U);
}
