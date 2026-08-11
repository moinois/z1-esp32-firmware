// Verifies exact DIAG-037 playback diagnostic formatting.
#include "test.hpp"

#include "application/diagnostics/playback_diagnostics.hpp"

#include <string>

using firmware::application::PlaybackDiagnosticEvent;

TEST_CASE(diag_037_formats_preparation_start_and_path_records) {
    using firmware::application::playback_diagnostic;
    REQUIRE_EQ(playback_diagnostic(
                   PlaybackDiagnosticEvent::preparation_recognized).tag,
               std::string_view("APP_ROUTER"));
    REQUIRE_EQ(playback_diagnostic(
                   PlaybackDiagnosticEvent::path_extracted, "/jobs/a.gcode").message,
               std::string("play命令原始文件名: '/jobs/a.gcode'"));
    REQUIRE(playback_diagnostic(
                PlaybackDiagnosticEvent::open_failure, "/sd/jobs/a.gcode").error);
    REQUIRE_EQ(playback_diagnostic(
                   PlaybackDiagnosticEvent::start_invalid).message,
               std::string("PLAY_FAIL[P1] start check failed.file does not exist or CRC wrong"));
}

TEST_CASE(diag_037_dequeue_uses_zero_or_payload_frame_number) {
    using firmware::application::playback_dequeue_diagnostic;
    REQUIRE_EQ(playback_dequeue_diagnostic(12U, 0xf1U, {}).message,
               std::string("PLAYQ_DEQ us=12 ty=0xF1 fn=0"));
    const firmware::core::ByteVector payload{1U, 2U, 0x12U, 0x34U, 0x56U, 0x78U};
    REQUIRE_EQ(playback_dequeue_diagnostic(99U, 0xf3U, payload).message,
               std::string("PLAYQ_DEQ us=99 ty=0xF3 fn=305419896"));
}
