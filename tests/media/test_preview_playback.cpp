// Verifies preview playback command transitions and clamped seeking.
#include "test.hpp"
#include "application/playback/preview_playback.hpp"

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

TEST_CASE(prev_021_inactive_open_is_silent_but_other_commands_conflict) {
    firmware::application::PreviewRequest request{};
    firmware::application::PreviewMode mode = firmware::application::PreviewMode::stopped;
    std::uint32_t frame = 0U;

    request.command = firmware::application::PreviewCommand::open;
    auto result = firmware::application::apply_preview_command(
        request, mode, frame, 2U, 40000U, true, false);
    REQUIRE(!result.reply);
    request.command = firmware::application::PreviewCommand::stop;
    result = firmware::application::apply_preview_command(
        request, mode, frame, 2U, 40000U, true, false);
    REQUIRE(result.reply);
    REQUIRE_EQ(result.response, std::string("conflict"));
}

TEST_CASE(prev_022_non_owner_paused_session_only_allows_resume_play_or_stop) {
    firmware::application::PreviewRequest request{};
    firmware::application::PreviewMode mode = firmware::application::PreviewMode::paused;
    std::uint32_t frame = 1U;

    request.command = firmware::application::PreviewCommand::seek;
    auto result = firmware::application::apply_preview_command(
        request, mode, frame, 5U, 40000U, false, true);
    REQUIRE(!result.reply);
    REQUIRE_EQ(mode, firmware::application::PreviewMode::paused);

    request.command = firmware::application::PreviewCommand::stop;
    result = firmware::application::apply_preview_command(
        request, mode, frame, 5U, 40000U, false, true);
    REQUIRE(result.terminated);
    REQUIRE(!result.reply);
    REQUIRE_EQ(mode, firmware::application::PreviewMode::stopped);
}

TEST_CASE(prev_023_to_027_time_selection_resume_and_stop_transitions) {
    firmware::application::PreviewRequest request{};
    firmware::application::PreviewMode mode = firmware::application::PreviewMode::stopped;
    std::uint32_t frame = 0U;

    request.command = firmware::application::PreviewCommand::play;
    request.from_frame = -1;
    request.from_milliseconds = 160U;
    auto result = firmware::application::apply_preview_command(
        request, mode, frame, 10U, 40000U, true, true);
    REQUIRE(result.reply);
    REQUIRE_EQ(frame, 4U);

    request.command = firmware::application::PreviewCommand::resume;
    result = firmware::application::apply_preview_command(
        request, mode, frame, 10U, 40000U, true, true);
    REQUIRE(result.reply);
    REQUIRE_EQ(mode, firmware::application::PreviewMode::playing);

    request.command = firmware::application::PreviewCommand::pause;
    mode = firmware::application::PreviewMode::paused;
    result = firmware::application::apply_preview_command(
        request, mode, frame, 10U, 40000U, true, true);
    REQUIRE(!result.reply);

    request.command = firmware::application::PreviewCommand::seek;
    request.frame = -1;
    request.time_milliseconds = 500U;
    result = firmware::application::apply_preview_command(
        request, mode, frame, 10U, 0U, true, true);
    REQUIRE(result.reply);
    REQUIRE_EQ(frame, 0U);
    REQUIRE_EQ(result.time_milliseconds, 0U);

    request.command = firmware::application::PreviewCommand::stop;
    mode = firmware::application::PreviewMode::playing;
    result = firmware::application::apply_preview_command(
        request, mode, frame, 10U, 40000U, true, true);
    REQUIRE(result.reply);
    REQUIRE(result.terminated);
}
