// Tests controller snapshot validation and the generated machine-status extension.
#include "test.hpp"
#include "core/protocol/status.hpp"

TEST_CASE(stat_005_status_extension_replaces_first_closing_bracket) {
    firmware::core::StatusExtension extension{};
    extension.transfer_active = true;
    extension.record_requested = false;
    extension.recording = true;
    extension.sd_used_mib = 12;
    extension.sd_total_mib = 34;
    extension.update_phase = 2;
    extension.update_progress = 75;
    REQUIRE_EQ(firmware::core::extend_status("<Idle|X:1>ignored", extension),
               std::string("<Idle|X:1|E:1,0,1,12,34|OTA:2,75>\n"));
}

TEST_CASE(stat_005_invalid_or_oversized_status_has_no_reply) {
    REQUIRE(!firmware::core::extend_status("Idle", {}).has_value());
    const std::string long_status = "<" + std::string(520, 'x') + ">";
    REQUIRE(!firmware::core::extend_status(long_status, {}).has_value());
}

TEST_CASE(stat_006_planner_marker_detects_machine_running) {
    REQUIRE(firmware::core::status_reports_running("<Idle|P:1>"));
    REQUIRE(!firmware::core::status_reports_running("<Idle|p:1>"));
}
