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

TEST_CASE(diag_021_formats_each_controller_receive_overflow) {
    using firmware::application::controller_receive_queue_full_diagnostic;
    REQUIRE_EQ(controller_receive_queue_full_diagnostic(0xc2U, 1U, 32U),
               std::string("LFU接收队列已满,丢弃数据"));
    REQUIRE_EQ(controller_receive_queue_full_diagnostic(0xd3U, 1U, 32U),
               std::string("CFG接收队列已满,丢弃数据"));
    REQUIRE_EQ(controller_receive_queue_full_diagnostic(0xe4U, 1U, 32U),
               std::string("FAC接收队列已满,丢弃数据"));
    REQUIRE_EQ(controller_receive_queue_full_diagnostic(0xf5U, 123U, 32U),
               std::string("PLAYQ_DROP us=123 ty=0xF5 qw=32"));
    REQUIRE_EQ(firmware::application::controller_host_output_purge_diagnostic(),
               std::string_view("到Controller的转发接收队列已满，丢弃数据，清空队列"));
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

TEST_CASE(diag_035_allocation_failures_use_exact_family_tag_and_text) {
    using firmware::application::controller_transfer_diagnostic;
    const auto data = controller_transfer_diagnostic(
        ControllerTransferFamily::firmware,
        ControllerTransferDiagnosticEvent::frame_data_allocation_failure);
    REQUIRE(data.error);
    REQUIRE_EQ(data.tag, std::string_view("dfu_LPC1768"));
    REQUIRE_EQ(data.message,
               std::string("Failed to allocate memory for frame data"));
    REQUIRE_EQ(controller_transfer_diagnostic(
                   ControllerTransferFamily::factory,
                   ControllerTransferDiagnosticEvent::encoded_frame_allocation_failure)
                   .message,
               std::string("Failed to allocate memory for frame"));
}
