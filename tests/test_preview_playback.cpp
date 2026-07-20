// Verifies preview playback command transitions and clamped seeking.
#include "test.hpp"
#include "firmware/application/preview_playback.hpp"

TEST_CASE(prev_023_to_027_playback_transitions) {
    firmware::application::PreviewRequest request{};
    request.command = firmware::application::PreviewCommand::play;
    request.from_frame = 99;
    firmware::application::PreviewMode mode = firmware::application::PreviewMode::paused;
    std::uint32_t frame = 2U;
    auto result = firmware::application::apply_preview_command(request, mode, frame, 10U, 40000U, true, true);
    REQUIRE(result.reply);
    REQUIRE_EQ(frame, 10U);
    REQUIRE_EQ(mode, firmware::application::PreviewMode::playing);
    request.command = firmware::application::PreviewCommand::pause;
    result = firmware::application::apply_preview_command(request, mode, frame, 10U, 40000U, true, true);
    REQUIRE(result.reply);
    REQUIRE_EQ(mode, firmware::application::PreviewMode::paused);
    request.command = firmware::application::PreviewCommand::seek;
    request.frame = 3;
    result = firmware::application::apply_preview_command(request, mode, frame, 10U, 40000U, true, true);
    REQUIRE_EQ(frame, 3U);
    REQUIRE_EQ(result.time_milliseconds, 120U);
}

TEST_CASE(prev_021_and_022_wrong_session_policy) {
    firmware::application::PreviewRequest request{};
    request.command = firmware::application::PreviewCommand::play;
    firmware::application::PreviewMode mode = firmware::application::PreviewMode::playing;
    std::uint32_t frame = 0U;
    auto ignored = firmware::application::apply_preview_command(request, mode, frame, 2U, 40000U, false, true);
    REQUIRE(!ignored.reply);
    const auto conflict = firmware::application::apply_preview_command(request, mode, frame, 2U, 40000U, false, false);
    REQUIRE(conflict.reply);
}
