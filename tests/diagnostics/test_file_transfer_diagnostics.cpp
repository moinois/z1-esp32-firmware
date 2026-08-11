// Verifies exact host file-transfer diagnostics from DIAG-027.
#include "test.hpp"

#include "application/diagnostics/file_transfer_diagnostics.hpp"

#include <string>

using firmware::application::file_transfer_busy_message;
using firmware::application::HostIdentity;
using firmware::application::HostTransport;

TEST_CASE(diag_027_busy_reply_identifies_the_tcp_client_in_uppercase_hex) {
    REQUIRE_EQ(file_transfer_busy_message({HostTransport::tcp, 0U, 7U}),
               std::string("发送 upload/download 忙回复(0x91)给客户端[0x10]"));
    REQUIRE_EQ(file_transfer_busy_message({HostTransport::tcp, 3U, 9U}),
               std::string("发送 upload/download 忙回复(0x91)给客户端[0x13]"));
}

TEST_CASE(diag_027_busy_reply_uses_the_normative_usb_source_identifier) {
    REQUIRE_EQ(file_transfer_busy_message({HostTransport::usb, 0U, 0U}),
               std::string("发送 upload/download 忙回复(0x91)给客户端[0x01]"));
}
