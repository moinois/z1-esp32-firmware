/** @file @brief Declares exact streamed-play diagnostic formatting. */
#pragma once

#include "core/protocol/bytes.hpp"

#include <cstddef>
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
    data_invalid,
    data_format_invalid,
    data_missing_frame_max,
    goto_invalid,
    goto_format_invalid,
    long_line_replaced,
    frame_allocation_failed,
    output_full,
    data_no_memory,
    goto_no_memory,
    host_broadcast_overflow,
    retained_cache_invalid,
};

/// Formats one fixed or path-bearing DIAG-037 playback record.
PlaybackDiagnostic playback_diagnostic(PlaybackDiagnosticEvent event,
                                       std::string_view path = {});

/// Formats the play-family dequeue record and derives `fn` from the payload.
PlaybackDiagnostic playback_dequeue_diagnostic(std::uint64_t microseconds,
                                               std::uint8_t frame_type,
                                               core::BytesView payload);

/// Formats one DIAG-037 sequence record carrying request and local counters.
PlaybackDiagnostic playback_sequence_diagnostic(std::string_view format,
                                                 std::uint32_t first,
                                                 std::uint32_t second,
                                                 std::uint32_t third = 0U,
                                                 std::uint32_t fourth = 0U,
                                                 bool warning = true);

/// Formats the exact successful F3 encoding record.
PlaybackDiagnostic playback_data_sent_diagnostic(std::size_t data_length);

/** Formats a failed streamed-play line-position consistency check.
 * @param path Selection path: `RETRANSMIT`, `SEEK`, or `SEQ_MATCH`.
 * @param request Requested logical-line index.
 * @param expected Expected current-line value after the operation.
 * @param actual Observed current-line value after the operation.
 * @param lines_sent Logical results consumed by this operation.
 * @param local_before Current-line value before processing began.
 */
PlaybackDiagnostic playback_invariant_diagnostic(
    std::string_view path, std::uint32_t request, std::uint32_t expected,
    std::uint32_t actual, std::int32_t lines_sent,
    std::uint32_t local_before);

}  // namespace firmware::application
