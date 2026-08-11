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

TEST_CASE(diag_037_formats_all_fixed_playback_failure_records_exactly) {
    using firmware::application::playback_diagnostic;
    const auto data_invalid = playback_diagnostic(PlaybackDiagnosticEvent::data_invalid);
    REQUIRE(data_invalid.error);
    REQUIRE_EQ(data_invalid.message,
               std::string("PLAY_FAIL[P2] data check failed.file does not exist or CRC wrong"));
    REQUIRE_EQ(playback_diagnostic(
                   PlaybackDiagnosticEvent::data_format_invalid).message,
               std::string("PLAY_FAIL [P2] PTYPE_PLAY_DATA command data format error"));
    REQUIRE_EQ(playback_diagnostic(
                   PlaybackDiagnosticEvent::data_missing_frame_max).message,
               std::string("PTYPE_PLAY_DATA command data format error (no frame_max field)"));
    REQUIRE_EQ(playback_diagnostic(PlaybackDiagnosticEvent::goto_invalid).message,
               std::string("PLAY_FAIL [P3]goto check failed.file does not exist or CRC wrong"));
    REQUIRE_EQ(playback_diagnostic(
                   PlaybackDiagnosticEvent::goto_format_invalid).message,
               std::string("[P3] PTYPE_PLAY_DATA goto cmd format error"));
    REQUIRE(playback_diagnostic(
                PlaybackDiagnosticEvent::long_line_replaced).warning);
    const auto overflow = playback_diagnostic(
        PlaybackDiagnosticEvent::host_broadcast_overflow);
    REQUIRE(overflow.warning);
    REQUIRE_EQ(overflow.tag, std::string_view("play_LPC1768"));
    REQUIRE_EQ(overflow.message,
               std::string("xRx2ControllerQueue full, drop 0x90 and reset queue"));
}

TEST_CASE(diag_037_formats_sequence_and_data_sent_records_exactly) {
    using firmware::application::playback_data_sent_diagnostic;
    using firmware::application::playback_sequence_diagnostic;
    REQUIRE_EQ(playback_sequence_diagnostic(
                   "FRAME_SEQ_RETRANSMIT req=%lu local=%lu", 9U, 12U).message,
               std::string("FRAME_SEQ_RETRANSMIT req=9 local=12"));
    const auto informational = playback_sequence_diagnostic(
        "FRAME_SEQ[GOTO] reached target: currentlen=%lu req=%lu local=%lu",
        64U, 3U, 3U, 0U, false);
    REQUIRE(!informational.warning);
    REQUIRE_EQ(informational.message,
               std::string("FRAME_SEQ[GOTO] reached target: currentlen=64 req=3 local=3"));
    REQUIRE_EQ(playback_data_sent_diagnostic(511U).message,
               std::string("Sent frame: type=0xF3, data_len=511"));
}
