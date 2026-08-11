/** @file @brief Declares exact DIAG-041 live-media resource diagnostics. */
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace firmware::application {

/// Resource failures possible while starting the live-media service or stream.
enum class LiveMediaDiagnosticEvent {
    arbiter_queue_create_failed,
    arbiter_task_create_failed,
    request_timeout,
    stream_allocation_failed,
    stream_task_create_failed,
};

/// One exact `media_mux` diagnostic with its normative severity.
struct LiveMediaDiagnostic {
    bool warning;
    std::string_view tag;
    std::string message;
};

/// Formats one fixed or socket-bearing DIAG-041 record.
LiveMediaDiagnostic live_media_diagnostic(LiveMediaDiagnosticEvent event,
                                          std::int32_t socket = 0);

}  // namespace firmware::application
