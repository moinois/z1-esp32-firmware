/** @file @brief Implements exact streamed-play diagnostic formatting. */
#include "application/diagnostics/playback_diagnostics.hpp"

#include <cstdio>

namespace firmware::application {

PlaybackDiagnostic playback_diagnostic(PlaybackDiagnosticEvent event,
                                       std::string_view path) {
    switch (event) {
        case PlaybackDiagnosticEvent::preparation_recognized:
            return {false, false, "APP_ROUTER", "收到了play命令准备处理"};
        case PlaybackDiagnosticEvent::path_extracted:
            return {false, false, "play_LPC1768",
                    "play命令原始文件名: '" + std::string(path) + "'"};
        case PlaybackDiagnosticEvent::open_failure:
            return {false, true, "play_LPC1768",
                    "PLAY_FAIL[P0_OPEN] fopen failed file='" +
                        std::string(path) + "'"};
        case PlaybackDiagnosticEvent::start_received:
            return {false, false, "play_LPC1768", "Received PTYPE_PLAY_START"};
        case PlaybackDiagnosticEvent::start_invalid:
            return {false, true, "play_LPC1768",
                    "PLAY_FAIL[P1] start check failed.file does not exist or CRC wrong"};
        case PlaybackDiagnosticEvent::start_valid:
            return {false, false, "play_LPC1768", "Send PTYPE_PLAY_VIEW to LPC1768"};
        case PlaybackDiagnosticEvent::data_invalid:
            return {false, true, "play_LPC1768",
                    "PLAY_FAIL[P2] data check failed.file does not exist or CRC wrong"};
        case PlaybackDiagnosticEvent::data_format_invalid:
            return {false, true, "play_LPC1768",
                    "PLAY_FAIL [P2] PTYPE_PLAY_DATA command data format error"};
        case PlaybackDiagnosticEvent::data_missing_frame_max:
            return {false, true, "play_LPC1768",
                    "PTYPE_PLAY_DATA command data format error (no frame_max field)"};
        case PlaybackDiagnosticEvent::goto_invalid:
            return {false, true, "play_LPC1768",
                    "PLAY_FAIL [P3]goto check failed.file does not exist or CRC wrong"};
        case PlaybackDiagnosticEvent::goto_format_invalid:
            return {false, true, "play_LPC1768",
                    "[P3] PTYPE_PLAY_DATA goto cmd format error"};
        case PlaybackDiagnosticEvent::long_line_replaced:
            return {true, false, "play_LPC1768",
                    "Long line replaced with empty line for LPC1768"};
        case PlaybackDiagnosticEvent::frame_allocation_failed:
            return {false, true, "play_LPC1768",
                    "Failed to allocate memory for frame"};
        case PlaybackDiagnosticEvent::output_full:
            return {false, false, "play_LPC1768", "队列已满，放入队列失败"};
        case PlaybackDiagnosticEvent::data_no_memory:
            return {false, true, "play_LPC1768",
                    "PLAY_FAIL [P2] no memory for frame_data"};
        case PlaybackDiagnosticEvent::goto_no_memory:
            return {false, true, "play_LPC1768",
                    "PLAY_FAIL [P3] no memory for goto frame_data"};
        case PlaybackDiagnosticEvent::host_broadcast_overflow:
            return {true, false, "play_LPC1768",
                    "xRx2ControllerQueue full, drop 0x90 and reset queue"};
    }
    return {};
}

PlaybackDiagnostic playback_sequence_diagnostic(std::string_view format,
                                                 std::uint32_t first,
                                                 std::uint32_t second,
                                                 std::uint32_t third,
                                                 std::uint32_t fourth,
                                                 bool warning) {
    char message[192]{};
    std::snprintf(message, sizeof(message), std::string(format).c_str(),
                  static_cast<unsigned long>(first),
                  static_cast<unsigned long>(second),
                  static_cast<unsigned long>(third),
                  static_cast<unsigned long>(fourth));
    return {warning, false, "play_LPC1768", message};
}

PlaybackDiagnostic playback_data_sent_diagnostic(std::size_t data_length) {
    char message[80]{};
    std::snprintf(message, sizeof(message), "Sent frame: type=0xF3, data_len=%ld",
                  static_cast<long>(data_length));
    return {false, false, "play_LPC1768", message};
}

PlaybackDiagnostic playback_dequeue_diagnostic(std::uint64_t microseconds,
                                               std::uint8_t frame_type,
                                               core::BytesView payload) {
    std::uint32_t frame_number = 0U;
    if (payload.size() >= 6U) {
        frame_number = (std::uint32_t(payload[2]) << 24U) |
                       (std::uint32_t(payload[3]) << 16U) |
                       (std::uint32_t(payload[4]) << 8U) | payload[5];
    }
    char message[96]{};
    std::snprintf(message, sizeof(message),
                  "PLAYQ_DEQ us=%llu ty=0x%02X fn=%lu",
                  static_cast<unsigned long long>(microseconds),
                  static_cast<unsigned>(frame_type),
                  static_cast<unsigned long>(frame_number));
    return {false, false, "play_LPC1768", message};
}

}  // namespace firmware::application
