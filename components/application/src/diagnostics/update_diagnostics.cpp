/** @file @brief Implements exact aggregate-update diagnostic formatting. */
#include "application/diagnostics/update_diagnostics.hpp"

#include <cstdio>

namespace firmware::application {
namespace {

std::uint32_t read_le32(core::BytesView bytes, std::size_t offset) {
    if (offset + 4U > bytes.size()) return 0U;
    return bytes[offset] | (std::uint32_t(bytes[offset + 1U]) << 8U) |
           (std::uint32_t(bytes[offset + 2U]) << 16U) |
           (std::uint32_t(bytes[offset + 3U]) << 24U);
}

std::string hex(std::uint32_t value, unsigned width) {
    char output[11]{};
    if (width == 2U) {
        std::snprintf(output, sizeof(output), "0x%02X",
                      static_cast<unsigned>(value));
    } else {
        std::snprintf(output, sizeof(output), "0x%08lX",
                      static_cast<unsigned long>(value));
    }
    return output;
}

}  // namespace

std::vector<std::string> aggregate_header_diagnostics(
    const core::UpdateHeader& header, core::BytesView encoded_header) {
    return {"=== Aggregate Firmware Header Info ===",
            "Magic: 0x4D5173EE",
            "Header Version: " + std::to_string(header.version),
            "Header Length: 32 bytes",
            "FW Flags: " + hex(header.flags, 2U),
            "ESP32 Size: " + std::to_string(header.mainboard_size) + " bytes",
            "LPC1768 Size: " + std::to_string(header.controller_size) + " bytes",
            "ESP32 Version: " + hex(header.mainboard_version, 8U),
            "LPC1768 Version: " + hex(header.controller_version, 8U),
            "Header CRC32: " + hex(read_le32(encoded_header, 24U), 8U),
            "File CRC32: " + hex(read_le32(encoded_header, 28U), 8U),
            std::string("ESP32 Included: ") + (header.mainboard_size ? "Yes" : "No"),
            std::string("LPC1768 Included: ") + (header.controller_size ? "Yes" : "No"),
            "======================================"};
}

std::optional<UpdateRecoveryDiagnostic> update_recovery_diagnostic(
    std::uint8_t phase, bool staged_controller_exists) {
    switch (phase) {
        case 1U: return UpdateRecoveryDiagnostic{true, "OTA-NVS (phase=1)"};
        case 2U:
            return UpdateRecoveryDiagnostic{
                !staged_controller_exists,
                staged_controller_exists
                    ? "OTA-NVS (phase=2): resume LPC upgrade (lpc1768.bin present)"
                    : "OTA-NVS (phase=2): LPC phase but bin missing"};
        case 3U:
            return UpdateRecoveryDiagnostic{true,
                "OTA-NVS (phase=3): previous upgrade failed"};
        case 4U:
            return UpdateRecoveryDiagnostic{false,
                "OTA-NVS (phase=4): previous LPC transfer completed, clear NVS"};
        default: return std::nullopt;
    }
}

std::string ota_failure_diagnostic(int error) {
    char output[40]{};
    std::snprintf(output, sizeof(output), "ESP32 OTA failed: 0x%x",
                  static_cast<unsigned>(error));
    return output;
}

std::string update_nvs_open_failure(std::string_view error_name) {
    return "ota nvs open failed: " + std::string(error_name);
}

std::string update_nvs_save_failure(std::uint8_t phase,
                                    std::string_view error_name) {
    return "ota nvs save phase " + std::to_string(phase) + " failed: " +
           std::string(error_name);
}

std::string update_delete_mode_failure(std::string_view path, int error,
                                       std::string_view error_text) {
    return "[fw_del] chmod 失败: path=" + std::string(path) + " errno=" +
           std::to_string(error) + " (" + std::string(error_text) + ")";
}

std::string update_delete_unrecoverable(std::string_view path, int error,
                                        std::string_view error_text) {
    return "[fw_del] 不可恢复错误，停止重试: path=" + std::string(path) +
           " errno=" + std::to_string(error) + " (" +
           std::string(error_text) + ")";
}

}  // namespace firmware::application
