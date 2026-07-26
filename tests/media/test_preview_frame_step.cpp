// Verifies frame delivery ordering, timing rounding, and EOS suppression rules.
#include "test.hpp"
#include "firmware/application/preview_frame_step.hpp"

TEST_CASE(prev_028_frame_step_sends_and_rounds_to_ten_millisecond_ticks) {
    const auto step = firmware::application::schedule_preview_frame(
        2U, 4U, 45000U, true, true, true, false);
    REQUIRE_EQ(step.action, firmware::application::PreviewFrameAction::send_frame);
    REQUIRE_EQ(step.frame_index, 2U);
    REQUIRE_EQ(step.delay_milliseconds, 40U);
    const auto minimum = firmware::application::schedule_preview_frame(
        0U, 1U, 5000U, true, true, true, false);
    REQUIRE_EQ(minimum.delay_milliseconds, 10U);
}

TEST_CASE(prev_029_eos_only_occurs_after_clean_end) {
    const auto eos = firmware::application::schedule_preview_frame(
        4U, 4U, 100000U, true, true, true, false);
    REQUIRE_EQ(eos.action, firmware::application::PreviewFrameAction::eos);
    const auto failed = firmware::application::schedule_preview_frame(
        4U, 4U, 100000U, false, true, true, false);
    REQUIRE_EQ(failed.action, firmware::application::PreviewFrameAction::terminate);
    const auto stopped = firmware::application::schedule_preview_frame(
        4U, 4U, 100000U, true, true, true, true);
    REQUIRE_EQ(stopped.action, firmware::application::PreviewFrameAction::terminate);
}
