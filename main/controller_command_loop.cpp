// Implements UART frame decoding and wall-clock command dispatch.
#include "controller_command_loop.hpp"

#include "controller_uart_adapter.hpp"
#include "controller_transfer_adapter.hpp"
#include "controller_play_adapter.hpp"
#include "play_runtime_state.hpp"
#include "runtime_status_adapter.hpp"
#include "wall_clock_adapter.hpp"
#include "wall_clock_command_dispatcher.hpp"
#include "serial_number_adapter.hpp"
#include "firmware/application/serial_number.hpp"
#include "firmware/application/recording_commands.hpp"
#include "recording_request_state.hpp"
#include "firmware/core/text.hpp"
#include "firmware/core/frame.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"

#include "firmware/application/controller_frame_forwarder.hpp"
#include "firmware/application/controller_query.hpp"
#include "firmware/application/controller_firmware_transfer.hpp"
#include "firmware/application/controller_config_transfer.hpp"
#include "firmware/application/controller_factory_transfer.hpp"
#include "firmware/application/local_command_queue.hpp"
#include "firmware/application/play_controller.hpp"
#include "firmware/core/protocol_constants.hpp"

#include <cstdint>
#include <optional>

namespace firmware::target {
namespace {

firmware::application::ControllerFrameForwarder controller_forwarder;
SemaphoreHandle_t controller_forwarder_mutex = nullptr;
firmware::application::ControllerFirmwareTransfer* active_firmware = nullptr;
firmware::application::ControllerConfigTransfer* active_configuration = nullptr;
firmware::application::ControllerFactoryTransfer* active_factory = nullptr;

void drain_forwarded_frames(ControllerUartAdapter& uart) {
    for (;;) {
        std::optional<firmware::core::ByteVector> item;
        if (xSemaphoreTake(controller_forwarder_mutex, portMAX_DELAY) == pdTRUE) {
            item = controller_forwarder.take_ready(
                static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL));
            xSemaphoreGive(controller_forwarder_mutex);
        }
        if (!item.has_value()) {
            return;
        }
        static_cast<void>(uart.write(*item));
    }
}

void controller_command_task(void*) {
    ControllerUartAdapter uart;
    if (!uart.initialize()) {
        vTaskDelete(nullptr);
        return;
    }
    EspWallClockAdapter wall_clock(&uart);
    WallClockCommandDispatcher dispatcher(wall_clock);
    NvsSerialNumberAdapter serial_port(&uart);
    firmware::application::SerialNumberService serial_service(serial_port);
    RecordingRequestState recording_state;
    ControllerTransferAdapter transfer_port(uart);
    firmware::application::ControllerFirmwareTransfer firmware_transfer;
    firmware::application::ControllerConfigTransfer config_transfer;
    firmware::application::ControllerFactoryTransfer factory_transfer;
    active_firmware = &firmware_transfer;
    active_configuration = &config_transfer;
    active_factory = &factory_transfer;
    auto& play_session = shared_play_session();
    firmware::application::PlayController play_controller(play_session);
    ControllerPlayAdapter play_port(uart);
    firmware::core::StreamDecoder decoder(
        firmware::core::StreamPolicy::controller_uart());
    firmware::application::LocalCommandQueue local_commands;
    firmware::application::ControllerQueryScheduler query_scheduler(
        static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL));
    std::uint8_t input[256];
    for (;;) {
        drain_forwarded_frames(uart);
        const auto due_queries = query_scheduler.poll(
            static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL), true);
        for (const auto& query : due_queries) {
            const auto encoded = firmware::core::encode_frame(query);
            if (!encoded.empty()) {
                static_cast<void>(uart.write(encoded));
            }
        }
        const int count = uart.read(input, sizeof(input));
        if (count <= 0) continue;
        const auto frames = decoder.push(
            {input, static_cast<std::size_t>(count)});
        for (const auto& frame : frames) {
            if (frame.type == firmware::core::protocol::machine_status) {
                shared_controller_snapshots().update_status(frame.payload);
            } else if (frame.type == firmware::core::protocol::diagnostic_data) {
                shared_controller_snapshots().update_diagnostic(frame.payload);
            } else if (frame.type == firmware::core::protocol::controller_version) {
                shared_controller_snapshots().update_version(frame.payload);
            }
            const std::uint8_t family =
                frame.type & firmware::core::protocol::family_mask;
            if (family == firmware::core::protocol::firmware_family) {
                firmware_transfer.handle(
                    frame, static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL),
                    transfer_port);
                continue;
            }
            if (family == firmware::core::protocol::configuration_family) {
                config_transfer.handle(frame, transfer_port);
                continue;
            }
            if (family == firmware::core::protocol::factory_family) {
                factory_transfer.handle(frame, transfer_port);
                continue;
            }
            if (family == firmware::core::protocol::play_family) {
                play_controller.handle(
                    frame, static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL),
                    play_port);
                continue;
            }
            dispatcher.dispatch(frame);
            if (frame.type != firmware::core::protocol::general_command) continue;
            const auto match = firmware::core::recognize_command(frame.payload);
            if (match.kind == firmware::core::CommandKind::serial_get ||
                match.kind == firmware::core::CommandKind::serial_set ||
                match.kind == firmware::core::CommandKind::record_start ||
                match.kind == firmware::core::CommandKind::record_stop) {
                static_cast<void>(local_commands.enqueue(frame));
            }
        }
        if (const auto local_frame = local_commands.dequeue();
            local_frame.has_value()) {
            const std::string_view command(
                reinterpret_cast<const char*>(local_frame->payload.data()),
                local_frame->payload.size());
            const auto match = firmware::core::recognize_command(
                local_frame->payload);
            if (match.kind == firmware::core::CommandKind::serial_get) {
                serial_service.handle_get(command);
            } else if (match.kind == firmware::core::CommandKind::serial_set) {
                serial_service.handle_set(command);
            } else {
                const auto result = firmware::application::handle_recording_command(
                    match.kind, recording_state.requested());
                recording_state.set_requested(result.requested);
                const auto encoded = firmware::core::encode_frame(result.response);
                if (!encoded.empty()) {
                    static_cast<void>(uart.write(encoded));
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10U));
        }
    }
}

}  // namespace

void ControllerCommandLoop::start() {
    if (controller_forwarder_mutex == nullptr) {
        controller_forwarder_mutex = xSemaphoreCreateMutex();
    }
    if (controller_forwarder_mutex == nullptr) {
        return;
    }
    xTaskCreate(controller_command_task, "controller_commands", 6144U, nullptr,
                5U, nullptr);
}

bool enqueue_controller_frame(const firmware::core::Frame& frame) {
    if (controller_forwarder_mutex == nullptr) {
        return false;
    }
    if (xSemaphoreTake(controller_forwarder_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    const bool queued = controller_forwarder.forward(frame);
    xSemaphoreGive(controller_forwarder_mutex);
    return queued;
}

bool controller_firmware_transfer_active() {
    return active_firmware != nullptr && active_firmware->active();
}

bool controller_configuration_transfer_active() {
    return active_configuration != nullptr && active_configuration->active();
}

bool controller_factory_transfer_active() {
    return active_factory != nullptr && active_factory->active();
}

}  // namespace firmware::target
