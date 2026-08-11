/** @file @brief Implements exact DIAG-043 runtime accounting diagnostics. */
#include "application/diagnostics/runtime_diagnostics.hpp"

namespace firmware::application {

std::string runtime_diagnostic(RuntimeDiagnosticEvent event,
                               std::string_view error_name) {
    switch (event) {
        case RuntimeDiagnosticEvent::first_boot_queue_full:
            return "stats TRY_FIRST_BOOT queue full";
        case RuntimeDiagnosticEvent::play_state_queue_full:
            return "stats PLAY_STATE queue full";
        case RuntimeDiagnosticEvent::persistence_queue_full:
            return "stats PERSIST_UPTIME queue full";
        case RuntimeDiagnosticEvent::first_boot_open_failed:
            return "try_first_boot: nvs_open failed: " + std::string(error_name);
        case RuntimeDiagnosticEvent::first_boot_write_failed:
            return "try_first_boot: nvs_set_i64 failed: " + std::string(error_name);
        case RuntimeDiagnosticEvent::power_on_open_failed:
            return "stats_commit_pon: nvs_open failed";
        case RuntimeDiagnosticEvent::machine_write_failed:
            return "play_state: nvs_set_u64 mach failed";
        case RuntimeDiagnosticEvent::machine_open_failed:
            return "play_state: mach nvs_open failed";
    }
    return {};
}

}  // namespace firmware::application
