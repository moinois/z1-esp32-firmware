/** @file @brief Implements exact DIAG-027 file-transfer diagnostics. */
#include "application/diagnostics/file_transfer_diagnostics.hpp"

namespace firmware::application {
namespace {

constexpr char uppercase_hexadecimal_digits[] = "0123456789ABCDEF";
constexpr std::uint8_t usb_client_id = 0x01U;
constexpr std::uint8_t first_tcp_client_id = 0x10U;

}  // namespace

std::string file_transfer_busy_message(const HostIdentity& host) {
    const std::uint8_t client_id =
        host.transport == HostTransport::usb
            ? usb_client_id
            : static_cast<std::uint8_t>(first_tcp_client_id + host.slot);
    std::string message =
        "发送 upload/download 忙回复(0x91)给客户端[0x00]";
    const std::size_t high_digit = message.size() - 3U;
    message[high_digit] = uppercase_hexadecimal_digits[client_id >> 4U];
    message[high_digit + 1U] =
        uppercase_hexadecimal_digits[client_id & 0x0fU];
    return message;
}

}  // namespace firmware::application
