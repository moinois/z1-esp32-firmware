// Verifies exact host file-transfer diagnostics from DIAG-027.
#include "test.hpp"

#include "application/diagnostics/file_transfer_diagnostics.hpp"

#include <string>

using firmware::application::file_transfer_busy_message;
using firmware::application::non_owner_file_data_diagnostic;
using firmware::application::HostIdentity;
using firmware::application::HostTransport;

TEST_CASE(diag_027_busy_reply_identifies_the_tcp_client_in_uppercase_hex) {
    REQUIRE_EQ(file_transfer_busy_message({HostTransport::tcp, 0U, 7U}),
               std::string("发送 upload/download 忙回复(0x91)给客户端[0x10]"));
    REQUIRE_EQ(file_transfer_busy_message({HostTransport::tcp, 3U, 9U}),
               std::string("发送 upload/download 忙回复(0x91)给客户端[0x13]"));
}

TEST_CASE(diag_038_non_owner_file_data_uses_transport_specific_exact_warning) {
    const auto usb = non_owner_file_data_diagnostic(
        {HostTransport::usb, 0U, 0U}, {HostTransport::tcp, 2U, 0U});
    REQUIRE_EQ(usb.tag, std::string_view("USB"));
    REQUIRE_EQ(usb.message,
               std::string("USB文件传输命令来自非owner，当前owner[0x12]，忽略"));

    const auto tcp = non_owner_file_data_diagnostic(
        {HostTransport::tcp, 3U, 7U}, {HostTransport::usb, 0U, 0U});
    REQUIRE_EQ(tcp.tag, std::string_view("WIFI"));
    REQUIRE_EQ(tcp.message,
               std::string("文件传输命令来自非owner客户端[0x13]，当前owner[0x01]，忽略"));
}

TEST_CASE(diag_039_file_queue_diagnostics_are_exact) {
    const auto start =
        firmware::application::file_transfer_start_queue_full_diagnostic();
    REQUIRE_EQ(start.tag, std::string_view("APP_FILE"));
    REQUIRE_EQ(start.message, std::string("文件传输请求队列已满，丢弃"));
    const auto download =
        firmware::application::download_delivery_drop_diagnostic();
    REQUIRE_EQ(download.message,
               std::string("download: xFileTransferQueue full, drop chunk"));
}

TEST_CASE(diag_040_path_resolution_diagnostics_are_exact) {
    using firmware::application::PathResolutionDiagnostic;
    using firmware::application::path_resolution_diagnostic;
    REQUIRE_EQ(path_resolution_diagnostic(
                   PathResolutionDiagnostic::invalid_arguments).message,
               std::string("absolute_from_relative: invalid args"));
    REQUIRE_EQ(path_resolution_diagnostic(
                   PathResolutionDiagnostic::absolute_path_too_long).message,
               std::string("absolute_from_relative: path too long"));
    REQUIRE_EQ(path_resolution_diagnostic(
                   PathResolutionDiagnostic::current_directory_failure).message,
               std::string("absolute_from_relative: getcwd failed"));
    REQUIRE_EQ(path_resolution_diagnostic(
                   PathResolutionDiagnostic::combined_path_too_long).message,
               std::string("absolute_from_relative: combined path too long"));
}

TEST_CASE(diag_027_busy_reply_uses_the_normative_usb_source_identifier) {
    REQUIRE_EQ(file_transfer_busy_message({HostTransport::usb, 0U, 0U}),
               std::string("发送 upload/download 忙回复(0x91)给客户端[0x01]"));
}
