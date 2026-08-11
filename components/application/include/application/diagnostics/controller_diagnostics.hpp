/** @file @brief Declares exact controller-path diagnostic formatting. */
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace firmware::application {

enum class ControllerTransferFamily { firmware, configuration, factory };
enum class ControllerTransferDiagnosticEvent {
    start,
    layout,
    data,
    data_request,
    data_sent,
};

/// Owns one fully formatted transfer diagnostic and its normative tag/severity.
struct ControllerTransferDiagnostic {
    bool error;
    std::string_view tag;
    std::string message;
};

/// Formats one DIAG-034 transfer lifecycle record.
ControllerTransferDiagnostic controller_transfer_diagnostic(
    ControllerTransferFamily family, ControllerTransferDiagnosticEvent event,
    std::uint32_t frame_index = 0U);

/// Formats the DIAG-038 warning emitted when the controller output FIFO is full.
std::string controller_queue_full_diagnostic(std::uint8_t frame_type);

}  // namespace firmware::application
