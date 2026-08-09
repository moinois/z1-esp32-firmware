/** @file @brief Implements deterministic controller responses over the production framing codec. */
#include "mock_controller_channel_adapter.hpp"

#include "core/protocol/protocol_constants.hpp"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <algorithm>
#include <string_view>

namespace firmware::target {
namespace {

constexpr char tag[] = "MOCK_CTRL";
constexpr std::string_view mock_version = "mock-controller-1";
constexpr std::string_view mock_status =
    "<Idle|MPos:0.0000,0.0000,0.0000,0.0000,0.0000|C:0,0,0,0>\n";
constexpr std::string_view mock_diagnostic =
    "{MOCK:1|UART:OK|CTRL:SIMULATED}\n";
constexpr std::size_t fragmented_read_size = 7U;
constexpr std::uint16_t transfer_data_size = 512U;
constexpr std::string_view transfer_command_prefix = "mock-transfer ";
constexpr std::string_view transfer_error_command_prefix =
    "mock-transfer-error ";
constexpr std::string_view transfer_timeout_command_prefix =
    "mock-transfer-timeout ";
constexpr std::string_view transfer_cancel_command_prefix =
    "mock-transfer-cancel ";
constexpr std::string_view time_command_prefix = "mock-time ";
constexpr std::string_view controller_command_prefix = "mock-command ";
constexpr std::uint64_t transfer_timeout_delay_milliseconds = 5200U;

// Converts text constants to owned protocol payloads.
firmware::core::ByteVector bytes(std::string_view text) {
    return {text.begin(), text.end()};
}

}  // namespace

bool MockControllerChannelAdapter::initialize() {
    if (initialized_) {
        return true;
    }
    initialized_ = true;
    diagnostic_ = mock_diagnostic;
    queue_response({firmware::core::protocol::controller_version,
                    bytes(mock_version)});
    ESP_LOGW(tag, "TEST BUILD: deterministic mock controller initialized");
    return true;
}

int MockControllerChannelAdapter::read(std::uint8_t* destination,
                                       std::size_t capacity) {
    if (!initialized_ || destination == nullptr || capacity == 0U) {
        return 0;
    }
    if (pending_input_.empty() && delayed_response_.has_value() &&
        esp_timer_get_time() >= delayed_response_due_microseconds_) {
        queue_response(std::move(*delayed_response_));
        delayed_response_.reset();
        const std::string_view name =
            transfer_kind_ == TransferKind::firmware ? "FIRMWARE" : "TRANSFER";
        diagnostic_ = std::string("{MOCK:1|UART:FRAGMENTED|CTRL:SIMULATED|XFER:") +
                      std::string(name) + ":TIMEOUT-INJECTED}\n";
        // The delayed unknown operation is now owned by the mainboard state
        // machine. Release only the mock-side fixture so a subsequent command
        // can prove whether host-to-controller routing was unsuppressed.
        transfer_kind_ = TransferKind::none;
        transfer_fault_ = TransferFault::none;
        transfer_family_ = 0U;
    }
    if (pending_input_.empty()) {
        vTaskDelay(pdMS_TO_TICKS(30U));
        return 0;
    }
    // Deliberately fragment the byte stream so every target run exercises the
    // production UART decoder across arbitrary frame boundaries.
    const std::size_t count = std::min(
        {capacity, pending_input_.size(), fragmented_read_size});
    for (std::size_t index = 0U; index < count; ++index) {
        destination[index] = pending_input_.front();
        pending_input_.pop_front();
    }
    return static_cast<int>(count);
}

int MockControllerChannelAdapter::write(firmware::core::BytesView frame) {
    if (!initialized_) {
        return -1;
    }
    for (const auto& decoded : decoder_.push(frame)) {
        handle_frame(decoded);
    }
    return static_cast<int>(frame.size());
}

void MockControllerChannelAdapter::handle_frame(
    const firmware::core::Frame& decoded) {
    const std::string_view payload(
        reinterpret_cast<const char*>(decoded.payload.data()),
        decoded.payload.size());
    if (decoded.type == firmware::core::protocol::single_command &&
        payload == "?") {
        queue_response({firmware::core::protocol::machine_status,
                        bytes(mock_status)});
    } else if (decoded.type == firmware::core::protocol::general_command &&
               payload.starts_with("diagnose")) {
        queue_response({firmware::core::protocol::diagnostic_data,
                        bytes(diagnostic_)});
    } else if (decoded.type == firmware::core::protocol::general_command &&
               (payload.starts_with(transfer_command_prefix) ||
                payload.starts_with(transfer_error_command_prefix) ||
                payload.starts_with(transfer_timeout_command_prefix) ||
                payload.starts_with(transfer_cancel_command_prefix))) {
        const TransferFault fault =
            payload.starts_with(transfer_error_command_prefix)
                ? TransferFault::malformed_geometry
                : payload.starts_with(transfer_timeout_command_prefix)
                      ? TransferFault::timeout
                      : payload.starts_with(transfer_cancel_command_prefix)
                            ? TransferFault::controller_cancel
                            : TransferFault::none;
        const std::string_view prefix =
            fault == TransferFault::malformed_geometry
                ? transfer_error_command_prefix
                : fault == TransferFault::timeout
                      ? transfer_timeout_command_prefix
                      : fault == TransferFault::controller_cancel
                            ? transfer_cancel_command_prefix
                            : transfer_command_prefix;
        const std::string_view requested = payload.substr(prefix.size());
        if (requested == "firmware") {
            start_transfer(TransferKind::firmware, fault);
        } else if (requested == "configuration") {
            start_transfer(TransferKind::configuration, fault);
        } else if (requested == "factory") {
            start_transfer(TransferKind::factory, fault);
        }
    } else if (decoded.type == firmware::core::protocol::general_command &&
               payload.starts_with(time_command_prefix)) {
        const std::string requested_time =
            std::string("time ") +
            std::string(payload.substr(time_command_prefix.size()));
        queue_response({firmware::core::protocol::general_command,
                        bytes(requested_time)});
    } else if (decoded.type == firmware::core::protocol::general_command &&
               payload.starts_with(controller_command_prefix)) {
        const std::string_view command = payload.substr(controller_command_prefix.size());
        if (command == "sn-get" || command == "clearftm" || command == "M951" ||
            command == "M952") {
            pending_command_name_ = std::string(command);
            diagnostic_ = std::string("{MOCK:1|UART:FRAGMENTED|CTRL:SIMULATED|CMD:") +
                          pending_command_name_ + ":RUNNING}\n";
            queue_response({firmware::core::protocol::general_command,
                            bytes(command)});
        }
    } else if ((decoded.type == firmware::core::protocol::text_response ||
                decoded.type == firmware::core::protocol::general_command) &&
               !pending_command_name_.empty()) {
        diagnostic_ = std::string("{MOCK:1|UART:FRAGMENTED|CTRL:SIMULATED|CMD:") +
                      pending_command_name_ + ":OK}\n";
        pending_command_name_.clear();
    } else if ((decoded.type & firmware::core::protocol::family_mask) ==
               transfer_family_) {
        handle_transfer_response(decoded);
    }
}

void MockControllerChannelAdapter::start_transfer(TransferKind kind,
                                                  TransferFault fault) {
    if (transfer_kind_ != TransferKind::none) {
        return;
    }
    transfer_kind_ = kind;
    transfer_fault_ = fault;
    transfer_family_ =
        kind == TransferKind::firmware
            ? firmware::core::protocol::firmware_family
            : kind == TransferKind::configuration
                  ? firmware::core::protocol::configuration_family
                  : firmware::core::protocol::factory_family;
    transfer_frame_count_ = 0U;
    transfer_index_ = 0U;
    diagnostic_ = "{MOCK:1|UART:FRAGMENTED|CTRL:SIMULATED|XFER:RUNNING}\n";
    queue_response({firmware::core::protocol::family_packet(
                        transfer_family_, firmware::core::protocol::transfer_start),
                    {}});
}

void MockControllerChannelAdapter::handle_transfer_response(
    const firmware::core::Frame& frame) {
    const std::uint8_t operation =
        frame.type & firmware::core::protocol::operation_mask;
    if (operation == firmware::core::protocol::transfer_cancel) {
        complete_transfer(false);
        return;
    }
    if (operation == firmware::core::protocol::transfer_start) {
        if (transfer_fault_ == TransferFault::malformed_geometry) {
            // A geometry packet must contain a four-byte frame count and a
            // two-byte block size. This intentionally short payload exercises
            // the production cancel path without corrupting the byte stream.
            queue_response({firmware::core::protocol::family_packet(
                                transfer_family_,
                                firmware::core::protocol::transfer_geometry),
                            {0U}});
            return;
        }
        if (transfer_fault_ == TransferFault::timeout) {
            // LPCFW-006 advances its timeout only when another family frame is
            // processed. Schedule an otherwise unused family operation after
            // five seconds; sleeping here would block the same UART task whose
            // timeout path the fixture is intended to exercise.
            schedule_response(
                {firmware::core::protocol::family_packet(transfer_family_, 0x0FU),
                 {}},
                transfer_timeout_delay_milliseconds);
            return;
        }
        queue_response({firmware::core::protocol::family_packet(
                            transfer_family_,
                            firmware::core::protocol::transfer_geometry),
                        {0U, 0U, 0U, 0U,
                         static_cast<std::uint8_t>(transfer_data_size >> 8U),
                         static_cast<std::uint8_t>(transfer_data_size)}});
        return;
    }
    if (operation == firmware::core::protocol::transfer_geometry &&
        frame.payload.size() >= 6U) {
        transfer_frame_count_ =
            (static_cast<std::uint32_t>(frame.payload[0]) << 24U) |
            (static_cast<std::uint32_t>(frame.payload[1]) << 16U) |
            (static_cast<std::uint32_t>(frame.payload[2]) << 8U) |
            static_cast<std::uint32_t>(frame.payload[3]);
        if (transfer_fault_ == TransferFault::controller_cancel) {
            complete_transfer(false);
        } else if (transfer_frame_count_ == 0U) {
            complete_transfer(true);
        } else {
            transfer_index_ = 1U;
            request_transfer_data();
        }
        return;
    }
    if (operation != firmware::core::protocol::transfer_data ||
        frame.payload.size() < 4U) {
        return;
    }
    const std::uint32_t received_index =
        (static_cast<std::uint32_t>(frame.payload[0]) << 24U) |
        (static_cast<std::uint32_t>(frame.payload[1]) << 16U) |
        (static_cast<std::uint32_t>(frame.payload[2]) << 8U) |
        static_cast<std::uint32_t>(frame.payload[3]);
    if (received_index != transfer_index_) {
        complete_transfer(false);
    } else if (transfer_index_ < transfer_frame_count_) {
        ++transfer_index_;
        request_transfer_data();
    } else {
        complete_transfer(true);
    }
}

void MockControllerChannelAdapter::request_transfer_data() {
    queue_response({
        firmware::core::protocol::family_packet(
            transfer_family_, firmware::core::protocol::transfer_data),
        {
            static_cast<std::uint8_t>(transfer_index_ >> 24U),
            static_cast<std::uint8_t>(transfer_index_ >> 16U),
            static_cast<std::uint8_t>(transfer_index_ >> 8U),
            static_cast<std::uint8_t>(transfer_index_),
        },
    });
}

void MockControllerChannelAdapter::complete_transfer(bool succeeded) {
    const std::uint8_t terminal_operation =
        succeeded ? firmware::core::protocol::transfer_complete
                  : firmware::core::protocol::transfer_cancel;
    queue_response({firmware::core::protocol::family_packet(
                        transfer_family_, terminal_operation),
                    {}});
    const std::string_view name =
        transfer_kind_ == TransferKind::firmware
            ? "FIRMWARE"
            : transfer_kind_ == TransferKind::configuration ? "CONFIGURATION"
                                                             : "FACTORY";
    diagnostic_ = std::string("{MOCK:1|UART:FRAGMENTED|CTRL:SIMULATED|XFER:") +
                  std::string(name) + (succeeded ? ":OK}\n" : ":ERROR}\n");
    transfer_kind_ = TransferKind::none;
    transfer_fault_ = TransferFault::none;
    transfer_family_ = 0U;
    transfer_frame_count_ = 0U;
    transfer_index_ = 0U;
    delayed_response_.reset();
}

void MockControllerChannelAdapter::schedule_response(
    firmware::core::Frame frame, std::uint64_t delay_milliseconds) {
    delayed_response_ = std::move(frame);
    delayed_response_due_microseconds_ =
        esp_timer_get_time() + static_cast<std::int64_t>(delay_milliseconds * 1000U);
}

void MockControllerChannelAdapter::queue_response(
    firmware::core::Frame frame) {
    const auto encoded = firmware::core::encode_controller_frame(frame);
    pending_input_.insert(pending_input_.end(), encoded.begin(), encoded.end());
}

}  // namespace firmware::target
