// Verifies preview open admission, bounded allocation, and session formatting.
#include "test.hpp"
#include "firmware/application/preview_open.hpp"

TEST_CASE(prev_010_to_012_open_admission_and_session_id) {
    firmware::core::AviPreview avi;
    avi.entries.resize(1U);
    avi.entries[0].advertised_size = 1000U;
    const auto decision = firmware::application::decide_preview_open(
        "/sd/videos/a.avi", &avi, true, true, 0x12ABU, 0x00EFU);
    REQUIRE_EQ(decision.error, firmware::application::PreviewOpenError::none);
    REQUIRE_EQ(decision.frame_buffer_size, 1000U);
    REQUIRE_EQ(decision.session_id, std::string("pv-000012ab-000000ef"));
}

TEST_CASE(prev_010_and_011_open_failures_are_ordered) {
    const auto path = firmware::application::decide_preview_open(
        "/tmp/a.avi", nullptr, false, false, 1U, 2U);
    REQUIRE_EQ(path.error, firmware::application::PreviewOpenError::path_not_allowed);
    const auto missing = firmware::application::decide_preview_open(
        "/sd/videos/a.avi", nullptr, false, false, 1U, 2U);
    REQUIRE_EQ(missing.error, firmware::application::PreviewOpenError::missing_file);
    const auto open = firmware::application::decide_preview_open(
        "/sd/videos/a.avi", nullptr, true, false, 1U, 2U);
    REQUIRE_EQ(open.error, firmware::application::PreviewOpenError::open_failed);
}

TEST_CASE(prev_015_buffer_policy_rejects_large_and_defaults_small) {
    firmware::core::AviPreview small;
    small.entries.resize(1U);
    small.entries[0].advertised_size = 511U;
    const auto low = firmware::application::decide_preview_open(
        "/sd/videos/a.avi", &small, true, true, 1U, 2U);
    REQUIRE_EQ(low.frame_buffer_size, 65536U);
    firmware::core::AviPreview large;
    large.entries.resize(1U);
    large.entries[0].advertised_size = 262145U;
    const auto high = firmware::application::decide_preview_open(
        "/sd/videos/a.avi", &large, true, true, 1U, 2U);
    REQUIRE_EQ(high.error, firmware::application::PreviewOpenError::buffer_too_large);
}
