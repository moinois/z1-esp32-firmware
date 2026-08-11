/** @file @brief Declares exact aggregate-update diagnostic formatting. */
#pragma once

#include "core/update/update_package.hpp"

#include <string>
#include <optional>
#include <vector>

namespace firmware::application {

/// Formats the ordered DIAG-030 records for an already validated header.
std::vector<std::string> aggregate_header_diagnostics(
    const core::UpdateHeader& header, core::BytesView encoded_header);

struct UpdateRecoveryDiagnostic {
    bool warning;
    std::string message;
};

/// Selects the single DIAG-031 record for a persisted update phase.
std::optional<UpdateRecoveryDiagnostic> update_recovery_diagnostic(
    std::uint8_t phase, bool staged_controller_exists);

/// Formats the DIAG-032 ESP-IDF OTA failure code in lowercase hexadecimal.
std::string ota_failure_diagnostic(int error);

}  // namespace firmware::application
