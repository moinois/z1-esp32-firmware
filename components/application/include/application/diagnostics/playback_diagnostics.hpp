/** @file @brief Declares exact streamed-play diagnostic formatting. */
#pragma once

#include "core/protocol/bytes.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace firmware::application {

/// Owns one DIAG-037 record and its normative severity/tag.
struct PlaybackDiagnostic {
    bool warning;
    bool error;
    std::string_view tag;
    std::string message;
};

enum class PlaybackDiagnosticEvent {
    preparation_recognized,
    path_extracted,
    open_failure,
    start_received,
    start_invalid,
    start_valid,
};

/// Formats one fixed or path-bearing DIAG-037 playback record.
PlaybackDiagnostic playback_diagnostic(PlaybackDiagnosticEvent event,
                                       std::string_view path = {});

/// Formats the play-family dequeue record and derives `fn` from the payload.
PlaybackDiagnostic playback_dequeue_diagnostic(std::uint64_t microseconds,
                                               std::uint8_t frame_type,
                                               core::BytesView payload);

}  // namespace firmware::application
