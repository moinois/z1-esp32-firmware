/** @file @brief Implements exact DIAG-041 live-media resource diagnostics. */
#include "application/diagnostics/live_media_diagnostics.hpp"

#include <cstdio>

namespace firmware::application {

LiveMediaDiagnostic live_media_diagnostic(LiveMediaDiagnosticEvent event,
                                          std::int32_t socket) {
    switch (event) {
        case LiveMediaDiagnosticEvent::arbiter_queue_create_failed:
            return {false, "media_mux", "arb queue create failed"};
        case LiveMediaDiagnosticEvent::arbiter_task_create_failed:
            return {false, "media_mux", "live_arbiter task create failed"};
        case LiveMediaDiagnosticEvent::request_timeout:
        case LiveMediaDiagnosticEvent::stream_allocation_failed:
        case LiveMediaDiagnosticEvent::stream_task_create_failed:
            break;
    }
    const char* format = nullptr;
    bool warning = false;
    if (event == LiveMediaDiagnosticEvent::request_timeout) {
        format = "begin live: arb queue full or send timeout fd=%ld";
        warning = true;
    } else if (event == LiveMediaDiagnosticEvent::stream_allocation_failed) {
        format = "live malloc failed fd=%ld";
    } else {
        format = "live task create failed fd=%ld";
    }
    char message[96]{};
    std::snprintf(message, sizeof(message), format, static_cast<long>(socket));
    return {warning, "media_mux", message};
}

}  // namespace firmware::application
