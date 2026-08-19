/** @file @brief Declares exact controller-path diagnostic formatting. */
#pragma once

#include "core/protocol/bytes.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace firmware::application {

enum class ControllerTransferFamily { firmware, configuration, factory };
enum class ControllerTransferDiagnosticEvent {
    start,
    layout,
    data,
    data_request,
    data_sent,
    short_layout,
    short_data,
    layout_reply_failure,
    missing_content,
    data_open_failure,
    frame_data_allocation_failure,
    encoded_frame_allocation_failure,
    timeout,
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

/// Formats the DIAG-035 record emitted once a response has been encoded.
ControllerTransferDiagnostic controller_transfer_sent_diagnostic(
    std::uint8_t frame_type, std::size_t payload_size);

/// Formats the ordered DIAG-036 layout metadata encoded in a response.
std::vector<ControllerTransferDiagnostic> controller_transfer_layout_diagnostics(
    std::uint8_t frame_type, core::BytesView payload);

/// Formats the DIAG-038 warning emitted when the controller output FIFO is full.
std::string controller_queue_full_diagnostic(std::uint8_t frame_type);

/// Formats the DIAG-021 receive-side overflow warning for one family inbox.
std::string controller_receive_queue_full_diagnostic(
    std::uint8_t frame_type, std::uint64_t microseconds,
    std::size_t pending_frames);

/// Returns the DIAG-021 warning for a purged ordinary controller broadcast.
std::string_view controller_host_output_purge_diagnostic();

}  // namespace firmware::application
