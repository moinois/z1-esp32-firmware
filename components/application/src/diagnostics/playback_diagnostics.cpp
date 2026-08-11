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
    }
    return {};
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
