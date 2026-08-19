/** @file @brief Implements exact DIAG-027 file-transfer diagnostics. */
#include "application/diagnostics/file_transfer_diagnostics.hpp"

#include <utility>

namespace firmware::application {
namespace {

constexpr char uppercase_hexadecimal_digits[] = "0123456789ABCDEF";
constexpr std::uint8_t usb_client_id = 0x01U;
constexpr std::uint8_t first_tcp_client_id = 0x10U;

std::uint8_t client_id(const HostIdentity& host) {
    return host.transport == HostTransport::usb
               ? usb_client_id
               : static_cast<std::uint8_t>(first_tcp_client_id + host.slot);
}

void replace_hex_id(std::string& message, std::size_t offset,
                    std::uint8_t value) {
    message[offset] = uppercase_hexadecimal_digits[value >> 4U];
    message[offset + 1U] = uppercase_hexadecimal_digits[value & 0x0fU];
}

}  // namespace

std::string file_transfer_busy_message(const HostIdentity& host) {
    const std::uint8_t id = client_id(host);
    std::string message =
        "发送 upload/download 忙回复(0x91)给客户端[0x00]";
    const std::size_t high_digit = message.size() - 3U;
    replace_hex_id(message, high_digit, id);
    return message;
}

FileTransferDiagnostic non_owner_file_data_diagnostic(
    const HostIdentity& source, const HostIdentity& owner) {
    if (source.transport == HostTransport::usb) {
        std::string message =
            "USB文件传输命令来自非owner，当前owner[0x00]，忽略";
        replace_hex_id(message, message.rfind("0x") + 2U, client_id(owner));
        return {"USB", std::move(message)};
    }
    std::string message =
        "文件传输命令来自非owner客户端[0x00]，当前owner[0x00]，忽略";
    const std::size_t owner_offset = message.rfind("0x") + 2U;
    const std::size_t source_offset = message.find("0x") + 2U;
    replace_hex_id(message, source_offset, client_id(source));
    replace_hex_id(message, owner_offset, client_id(owner));
    return {"WIFI", std::move(message)};
}

FileTransferDiagnostic file_transfer_start_queue_full_diagnostic() {
    return {"APP_FILE", "文件传输请求队列已满，丢弃"};
}

FileTransferDiagnostic file_transfer_request_storage_unavailable_diagnostic() {
    return {"APP_FILE", "文件传输队列未初始化，忽略请求"};
}

FileTransferDiagnostic download_delivery_drop_diagnostic() {
    return {"APP_FILE", "download: xFileTransferQueue full, drop chunk"};
}

FileTransferDiagnostic path_resolution_diagnostic(
    PathResolutionDiagnostic failure) {
    switch (failure) {
        case PathResolutionDiagnostic::invalid_arguments:
            return {"APP_FILE", "absolute_from_relative: invalid args"};
        case PathResolutionDiagnostic::absolute_path_too_long:
            return {"APP_FILE", "absolute_from_relative: path too long"};
        case PathResolutionDiagnostic::current_directory_failure:
            return {"APP_FILE", "absolute_from_relative: getcwd failed"};
        case PathResolutionDiagnostic::combined_path_too_long:
            return {"APP_FILE", "absolute_from_relative: combined path too long"};
    }
    return {};
}

}  // namespace firmware::application
