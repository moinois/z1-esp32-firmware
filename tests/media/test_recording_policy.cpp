// Verifies recording naming validation and segment interval accounting.
#include "test.hpp"
#include "application/web/recording_policy.hpp"

TEST_CASE(rec_003_and_004_segment_path_uses_utc_and_base_name) {
    const auto path = firmware::application::recording_segment_path(
        "/sd/job.v1/archive/ride.v2.avi", 0);
    REQUIRE(path.has_value());
    REQUIRE_EQ(*path, std::string("/sd/videos/ride-19700101_000000.avi"));
    REQUIRE(!firmware::application::recording_segment_path("ride.avi", 0).has_value());
    REQUIRE(!firmware::application::recording_segment_path("/sd/ride", 0).has_value());
}

TEST_CASE(rec_002_conditions_require_request_and_running_source) {
    REQUIRE(!firmware::application::recording_conditions_active(false, true, true));
    REQUIRE(!firmware::application::recording_conditions_active(true, false, false));
    REQUIRE(firmware::application::recording_conditions_active(true, true, false));
    REQUIRE(firmware::application::recording_conditions_active(true, false, true));
}

TEST_CASE(rec_007_to_010_attempts_and_inactive_intervals_close_segments) {
    firmware::application::RecordingSegmentState state;
    state.initialized = true;
    for (int interval = 0; interval < 30; ++interval) {
        REQUIRE(!firmware::application::advance_recording_segment(
            state, false, false, false, 100U));
    }
    REQUIRE(firmware::application::advance_recording_segment(
        state, false, false, false, 100U));
    firmware::application::RecordingSegmentState limited;
    limited.initialized = true;
    REQUIRE(firmware::application::advance_recording_segment(
        limited, true, true, true, 1U));
    REQUIRE_EQ(limited.successful_frames, 1U);
}
