// Verifies exact DIAG-030 aggregate-header diagnostics.
#include "test.hpp"

#include "application/diagnostics/update_diagnostics.hpp"

#include <string>
#include <vector>

using firmware::core::ByteVector;

TEST_CASE(diag_030_formats_every_valid_aggregate_header_line_in_order) {
    firmware::core::UpdateHeader header{2U, 3U, 17U, 9U, 0x1234abcdU,
                                        0x89abcdefU};
    ByteVector encoded(32U, 0U);
    encoded[24] = 0x78U; encoded[25] = 0x56U; encoded[26] = 0x34U; encoded[27] = 0x12U;
    encoded[28] = 0xefU; encoded[29] = 0xcdU; encoded[30] = 0xabU; encoded[31] = 0x89U;
    REQUIRE_EQ(firmware::application::aggregate_header_diagnostics(header, encoded),
        std::vector<std::string>({
            "=== Aggregate Firmware Header Info ===", "Magic: 0x4D5173EE",
            "Header Version: 2", "Header Length: 32 bytes", "FW Flags: 0x03",
            "ESP32 Size: 17 bytes", "LPC1768 Size: 9 bytes",
            "ESP32 Version: 0x1234ABCD", "LPC1768 Version: 0x89ABCDEF",
            "Header CRC32: 0x12345678", "File CRC32: 0x89ABCDEF",
            "ESP32 Included: Yes", "LPC1768 Included: Yes",
            "======================================"}));
}

TEST_CASE(diag_031_selects_the_exact_persisted_phase_record) {
    using firmware::application::update_recovery_diagnostic;
    REQUIRE_EQ(update_recovery_diagnostic(1U, false)->message,
               std::string("OTA-NVS (phase=1)"));
    REQUIRE_EQ(update_recovery_diagnostic(2U, true)->message,
               std::string("OTA-NVS (phase=2): resume LPC upgrade (lpc1768.bin present)"));
    REQUIRE_EQ(update_recovery_diagnostic(2U, false)->message,
               std::string("OTA-NVS (phase=2): LPC phase but bin missing"));
    REQUIRE_EQ(update_recovery_diagnostic(3U, false)->message,
               std::string("OTA-NVS (phase=3): previous upgrade failed"));
    REQUIRE_EQ(update_recovery_diagnostic(4U, false)->message,
               std::string("OTA-NVS (phase=4): previous LPC transfer completed, clear NVS"));
    REQUIRE(!update_recovery_diagnostic(0U, false).has_value());
    REQUIRE(!update_recovery_diagnostic(9U, false).has_value());
}

TEST_CASE(diag_032_formats_ota_errors_as_unpadded_lowercase_hex) {
    REQUIRE_EQ(firmware::application::ota_failure_diagnostic(0x10a),
               std::string("ESP32 OTA failed: 0x10a"));
}

TEST_CASE(diag_032_formats_update_nvs_failures_exactly) {
    REQUIRE_EQ(firmware::application::update_nvs_open_failure("ESP_FAIL"),
               std::string("ota nvs open failed: ESP_FAIL"));
    REQUIRE_EQ(firmware::application::update_nvs_save_failure(3U, "ESP_FAIL"),
               std::string("ota nvs save phase 3 failed: ESP_FAIL"));
}

TEST_CASE(diag_032_formats_update_delete_failures_exactly) {
    REQUIRE_EQ(firmware::application::update_delete_mode_failure(
                   "/sd/firmware.bin", 13, "Permission denied"),
               std::string("[fw_del] chmod 失败: path=/sd/firmware.bin errno=13 (Permission denied)"));
    REQUIRE_EQ(firmware::application::update_delete_unrecoverable(
                   "/sd/firmware.bin", 30, "Read-only file system"),
               std::string("[fw_del] 不可恢复错误，停止重试: path=/sd/firmware.bin errno=30 (Read-only file system)"));
}
