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

TEST_CASE(diag_027_busy_reply_uses_the_normative_usb_source_identifier) {
    REQUIRE_EQ(file_transfer_busy_message({HostTransport::usb, 0U, 0U}),
               std::string("发送 upload/download 忙回复(0x91)给客户端[0x01]"));
}
