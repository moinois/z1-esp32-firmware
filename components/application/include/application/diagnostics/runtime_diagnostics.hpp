/** @file @brief Declares exact DIAG-043 runtime accounting diagnostics. */
#pragma once

#include <string>
#include <string_view>

namespace firmware::application {

enum class RuntimeDiagnosticEvent {
    first_boot_queue_full,
    play_state_queue_full,
    persistence_queue_full,
    first_boot_open_failed,
    first_boot_write_failed,
    power_on_open_failed,
    machine_write_failed,
    machine_open_failed,
};

/// Formats one exact APP_NVS warning, optionally inserting an ESP error name.
std::string runtime_diagnostic(RuntimeDiagnosticEvent event,
                               std::string_view error_name = {});

}  // namespace firmware::application
