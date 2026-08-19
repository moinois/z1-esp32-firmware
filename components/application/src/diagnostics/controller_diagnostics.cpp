/** @file @brief Implements exact controller-path diagnostic formatting. */
#include "application/diagnostics/controller_diagnostics.hpp"

#include <cstdio>
#include <utility>

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
    } else if (event == ControllerTransferDiagnosticEvent::short_layout) {
        if (family == ControllerTransferFamily::firmware) {
            message = "PTYPE_FIRM_CAN response data format error";
        } else if (family == ControllerTransferFamily::configuration) {
            message = "PTYPE_CONFIG_CAN response data format error";
        } else {
            message = "PTYPE_FACTORY_VIEW response data format error";
        }
    } else if (event == ControllerTransferDiagnosticEvent::short_data) {
        message = family == ControllerTransferFamily::firmware
            ? "PTYPE_FIRM_DATA command data format error"
            : "PTYPE_CONFIG_CAN command data format error";
    } else if (event == ControllerTransferDiagnosticEvent::layout_reply_failure) {
        message = "Failed to send " + std::string(packet_name(
            family, ControllerTransferDiagnosticEvent::layout)) + " response";
    } else if (event == ControllerTransferDiagnosticEvent::missing_content) {
        if (family == ControllerTransferFamily::firmware) {
            message = "Firmware file does not exist: /sd/lpc1768.bin";
        } else if (family == ControllerTransferFamily::configuration) {
            message = "Config file does not exist: /sd/config.txt";
        } else {
            message = "Factory ini file does not exist: /sd/factory.ini";
        }
    } else if (event == ControllerTransferDiagnosticEvent::data_open_failure) {
        message = family == ControllerTransferFamily::factory
            ? "Failed to open Factory ini file: /sd/factory.ini"
            : "Failed to open firmware file: /sd/config.txt";
    } else if (event ==
               ControllerTransferDiagnosticEvent::frame_data_allocation_failure) {
        message = "Failed to allocate memory for frame data";
    } else if (event ==
               ControllerTransferDiagnosticEvent::encoded_frame_allocation_failure) {
        message = "Failed to allocate memory for frame";
    } else if (event == ControllerTransferDiagnosticEvent::timeout) {
        message = "DFU process timeout in state " + std::to_string(frame_index);
    } else {
        message = "Received " + std::string(packet_name(family, event));
    }
    const bool error = event == ControllerTransferDiagnosticEvent::short_layout ||
                       event == ControllerTransferDiagnosticEvent::short_data ||
                       event == ControllerTransferDiagnosticEvent::layout_reply_failure ||
                       event == ControllerTransferDiagnosticEvent::missing_content ||
                       event == ControllerTransferDiagnosticEvent::data_open_failure ||
                       event == ControllerTransferDiagnosticEvent::frame_data_allocation_failure ||
                       event == ControllerTransferDiagnosticEvent::encoded_frame_allocation_failure ||
                       event == ControllerTransferDiagnosticEvent::timeout;
    return {error, transfer_tag(family), std::move(message)};
}

ControllerTransferDiagnostic controller_transfer_sent_diagnostic(
    std::uint8_t frame_type, std::size_t payload_size) {
    const auto family = static_cast<ControllerTransferFamily>(
        ((frame_type >> 4U) - 0x0cU));
    char message[64]{};
    std::snprintf(message, sizeof(message),
                  "Sent frame: type=0x%02X, data_len=%d",
                  static_cast<unsigned>(frame_type),
                  static_cast<int>(payload_size));
    return {false, transfer_tag(family), message};
}

std::vector<ControllerTransferDiagnostic> controller_transfer_layout_diagnostics(
    std::uint8_t frame_type, core::BytesView payload) {
    if ((frame_type & 0x0fU) != 2U || payload.size() < 6U) return {};
    const std::uint32_t count = (std::uint32_t(payload[0]) << 24U) |
                                (std::uint32_t(payload[1]) << 16U) |
                                (std::uint32_t(payload[2]) << 8U) | payload[3];
    const std::uint16_t size = static_cast<std::uint16_t>(
        (std::uint16_t(payload[4]) << 8U) | payload[5]);
    const std::uint8_t family_nibble = frame_type & 0xf0U;
    if (family_nibble == 0xd0U) {
        return {{false, "cfg_LPC1768", "Total frames: " + std::to_string(count)},
                {false, "cfg_LPC1768", "Frame size: 512 bytes"}};
    }
    if (family_nibble == 0xe0U) {
        return {{false, "factory_LPC1768",
                 "Factory ini Total frames: " + std::to_string(count)},
                {false, "factory_LPC1768",
                 "Factory ini Frame size: " + std::to_string(size) + " bytes"}};
    }
    return {};
}

std::string controller_queue_full_diagnostic(std::uint8_t frame_type) {
    char output[48]{};
    std::snprintf(output, sizeof(output),
                  "TxQueue full, drop frame type=0x%02X",
                  static_cast<unsigned>(frame_type));
    return output;
}

std::string controller_receive_queue_full_diagnostic(
    std::uint8_t frame_type, std::uint64_t microseconds,
    std::size_t pending_frames) {
    switch (frame_type & 0xf0U) {
        case 0xc0U:
            return "LFU接收队列已满,丢弃数据";
        case 0xd0U:
            return "CFG接收队列已满,丢弃数据";
        case 0xe0U:
            return "FAC接收队列已满,丢弃数据";
        case 0xf0U: {
            char output[96]{};
            std::snprintf(output, sizeof(output),
                          "PLAYQ_DROP us=%llu ty=0x%02X qw=%u",
                          static_cast<unsigned long long>(microseconds),
                          static_cast<unsigned>(frame_type),
                          static_cast<unsigned>(pending_frames));
            return output;
        }
        default:
            return {};
    }
}

std::string_view controller_host_output_purge_diagnostic() {
    return "到Controller的转发接收队列已满，丢弃数据，清空队列";
}

}  // namespace firmware::application
