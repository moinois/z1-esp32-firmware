/** @file @brief Implements exact controller-path diagnostic formatting. */
#include "application/diagnostics/controller_diagnostics.hpp"

#include <cstdio>

namespace firmware::application {
namespace {

std::string_view transfer_tag(ControllerTransferFamily family) {
    switch (family) {
        case ControllerTransferFamily::firmware: return "dfu_LPC1768";
        case ControllerTransferFamily::configuration: return "cfg_LPC1768";
        case ControllerTransferFamily::factory: return "factory_LPC1768";
    }
    return {};
}

std::string_view packet_name(ControllerTransferFamily family,
                             ControllerTransferDiagnosticEvent event) {
    switch (family) {
        case ControllerTransferFamily::firmware:
            if (event == ControllerTransferDiagnosticEvent::start) return "PTYPE_FIRM_START";
            if (event == ControllerTransferDiagnosticEvent::layout) return "PTYPE_FIRM_VIEW";
            return "PTYPE_FIRM_DATA";
        case ControllerTransferFamily::configuration:
            if (event == ControllerTransferDiagnosticEvent::start) return "PTYPE_CONFIG_START";
            if (event == ControllerTransferDiagnosticEvent::layout) return "PTYPE_CONFIG_VIEW";
            return "PTYPE_CONFIG_DATA";
        case ControllerTransferFamily::factory:
            if (event == ControllerTransferDiagnosticEvent::start) return "PTYPE_FACTORY_START";
            if (event == ControllerTransferDiagnosticEvent::layout) return "PTYPE_FACTORY_VIEW";
            return "PTYPE_FACTORY_DATA";
    }
    return {};
}

}  // namespace

ControllerTransferDiagnostic controller_transfer_diagnostic(
    ControllerTransferFamily family, ControllerTransferDiagnosticEvent event,
    std::uint32_t frame_index) {
    std::string message;
    if (event == ControllerTransferDiagnosticEvent::data_request) {
        message = "Received device request for frame " +
                  std::to_string(frame_index) + " data";
    } else if (event == ControllerTransferDiagnosticEvent::data_sent) {
        message = "Frame " + std::to_string(frame_index) +
                  " data sent successfully";
    } else {
        message = "Received " + std::string(packet_name(family, event));
    }
    return {false, transfer_tag(family), std::move(message)};
}

std::string controller_queue_full_diagnostic(std::uint8_t frame_type) {
    char output[48]{};
    std::snprintf(output, sizeof(output),
                  "TxQueue full, drop frame type=0x%02X",
                  static_cast<unsigned>(frame_type));
    return output;
}

}  // namespace firmware::application
