// Verifies all exact DIAG-043 runtime-accounting warning messages.
#include "test.hpp"

#include "application/diagnostics/runtime_diagnostics.hpp"

#include <string>

using firmware::application::RuntimeDiagnosticEvent;
using firmware::application::runtime_diagnostic;

TEST_CASE(diag_043_runtime_queue_and_nvs_messages_are_exact) {
    REQUIRE_EQ(runtime_diagnostic(RuntimeDiagnosticEvent::first_boot_queue_full),
               std::string("stats TRY_FIRST_BOOT queue full"));
    REQUIRE_EQ(runtime_diagnostic(RuntimeDiagnosticEvent::play_state_queue_full),
               std::string("stats PLAY_STATE queue full"));
    REQUIRE_EQ(runtime_diagnostic(RuntimeDiagnosticEvent::persistence_queue_full),
               std::string("stats PERSIST_UPTIME queue full"));
    REQUIRE_EQ(runtime_diagnostic(RuntimeDiagnosticEvent::first_boot_open_failed,
                                  "ESP_ERR_NVS_NOT_ENOUGH_SPACE"),
               std::string("try_first_boot: nvs_open failed: ESP_ERR_NVS_NOT_ENOUGH_SPACE"));
    REQUIRE_EQ(runtime_diagnostic(RuntimeDiagnosticEvent::first_boot_write_failed,
                                  "ESP_FAIL"),
               std::string("try_first_boot: nvs_set_i64 failed: ESP_FAIL"));
    REQUIRE_EQ(runtime_diagnostic(RuntimeDiagnosticEvent::power_on_open_failed),
               std::string("stats_commit_pon: nvs_open failed"));
    REQUIRE_EQ(runtime_diagnostic(RuntimeDiagnosticEvent::machine_write_failed),
               std::string("play_state: nvs_set_u64 mach failed"));
    REQUIRE_EQ(runtime_diagnostic(RuntimeDiagnosticEvent::machine_open_failed),
               std::string("play_state: mach nvs_open failed"));
}
