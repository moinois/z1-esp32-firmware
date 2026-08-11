// Verifies exact controller-path diagnostics required by the specification.
#include "test.hpp"

#include "application/diagnostics/controller_diagnostics.hpp"

#include <string>

using firmware::application::ControllerTransferDiagnosticEvent;
using firmware::application::ControllerTransferFamily;

TEST_CASE(diag_034_formats_each_transfer_family_and_index_record) {
    using firmware::application::controller_transfer_diagnostic;
    REQUIRE_EQ(controller_transfer_diagnostic(
                   ControllerTransferFamily::firmware,
                   ControllerTransferDiagnosticEvent::start).tag,
               std::string_view("dfu_LPC1768"));
    REQUIRE_EQ(controller_transfer_diagnostic(
                   ControllerTransferFamily::configuration,
                   ControllerTransferDiagnosticEvent::layout).message,
               std::string("Received PTYPE_CONFIG_VIEW"));
    REQUIRE_EQ(controller_transfer_diagnostic(
                   ControllerTransferFamily::factory,
                   ControllerTransferDiagnosticEvent::data_request, 42U).message,
               std::string("Received device request for frame 42 data"));
    REQUIRE_EQ(controller_transfer_diagnostic(
                   ControllerTransferFamily::factory,
                   ControllerTransferDiagnosticEvent::data_sent, 42U).message,
               std::string("Frame 42 data sent successfully"));
}

TEST_CASE(diag_038_formats_controller_queue_full_type_as_two_uppercase_hex_digits) {
    REQUIRE_EQ(firmware::application::controller_queue_full_diagnostic(0x0aU),
               std::string("TxQueue full, drop frame type=0x0A"));
    REQUIRE_EQ(firmware::application::controller_queue_full_diagnostic(0xf3U),
               std::string("TxQueue full, drop frame type=0xF3"));
}

TEST_CASE(diag_035_and_036_format_sent_failures_and_layout_details) {
    using firmware::application::controller_transfer_diagnostic;
    REQUIRE_EQ(controller_transfer_diagnostic(
                   ControllerTransferFamily::firmware,
                   ControllerTransferDiagnosticEvent::short_layout).message,
               std::string("PTYPE_FIRM_CAN response data format error"));
    REQUIRE(controller_transfer_diagnostic(
                ControllerTransferFamily::factory,
                ControllerTransferDiagnosticEvent::missing_content).error);
    REQUIRE_EQ(firmware::application::controller_transfer_sent_diagnostic(
                   0xd2U, 6U).message,
               std::string("Sent frame: type=0xD2, data_len=6"));
    const firmware::core::ByteVector layout{0U, 0U, 0U, 3U, 2U, 0U};
    const auto details =
        firmware::application::controller_transfer_layout_diagnostics(0xd2U, layout);
    REQUIRE_EQ(details.size(), 2U);
    REQUIRE_EQ(details[0].message, std::string("Total frames: 3"));
    REQUIRE_EQ(details[1].message, std::string("Frame size: 512 bytes"));
}
