// Verifies exact DIAG-041 live-media resource diagnostic formatting.
#include "test.hpp"

#include "application/diagnostics/live_media_diagnostics.hpp"

#include <string>

using firmware::application::LiveMediaDiagnosticEvent;
using firmware::application::live_media_diagnostic;

TEST_CASE(diag_041_fixed_live_service_failures_use_media_mux) {
    const auto queue = live_media_diagnostic(
        LiveMediaDiagnosticEvent::arbiter_queue_create_failed);
    REQUIRE_EQ(queue.tag, std::string_view("media_mux"));
    REQUIRE_EQ(queue.message, std::string("arb queue create failed"));
    REQUIRE_EQ(live_media_diagnostic(
                   LiveMediaDiagnosticEvent::arbiter_task_create_failed).message,
               std::string("live_arbiter task create failed"));
}

TEST_CASE(diag_041_socket_failures_include_signed_descriptor) {
    const auto timeout = live_media_diagnostic(
        LiveMediaDiagnosticEvent::request_timeout, -7);
    REQUIRE(timeout.warning);
    REQUIRE_EQ(timeout.message,
               std::string("begin live: arb queue full or send timeout fd=-7"));
    REQUIRE_EQ(live_media_diagnostic(
                   LiveMediaDiagnosticEvent::stream_allocation_failed, 12).message,
               std::string("live malloc failed fd=12"));
    REQUIRE_EQ(live_media_diagnostic(
                   LiveMediaDiagnosticEvent::stream_task_create_failed, 9).message,
               std::string("live task create failed fd=9"));
}
