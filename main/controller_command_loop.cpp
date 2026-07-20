// Implements UART frame decoding and wall-clock command dispatch.
#include "controller_command_loop.hpp"

#include "controller_uart_adapter.hpp"
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

#include <cstdint>

namespace firmware::target {
namespace {

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
    firmware::core::StreamDecoder decoder(
        firmware::core::StreamPolicy::controller_uart());
    std::uint8_t input[256];
    for (;;) {
        const int count = uart.read(input, sizeof(input));
        if (count <= 0) continue;
        const auto frames = decoder.push(
            {input, static_cast<std::size_t>(count)});
        for (const auto& frame : frames) {
            dispatcher.dispatch(frame);
            if (frame.type != firmware::core::protocol::general_command) continue;
            const std::string_view command(
                reinterpret_cast<const char*>(frame.payload.data()),
                frame.payload.size());
            const auto match = firmware::core::recognize_command(frame.payload);
            if (match.kind == firmware::core::CommandKind::serial_get) {
                serial_service.handle_get(command);
            } else if (match.kind == firmware::core::CommandKind::serial_set) {
                serial_service.handle_set(command);
            } else if (match.kind == firmware::core::CommandKind::record_start ||
                       match.kind == firmware::core::CommandKind::record_stop) {
                const auto result = firmware::application::handle_recording_command(
                    match.kind, recording_state.requested());
                recording_state.set_requested(result.requested);
                const auto encoded = firmware::core::encode_frame(result.response);
                if (!encoded.empty()) uart.write(encoded);
            }
        }
    }
}

}  // namespace

void ControllerCommandLoop::start() {
    xTaskCreate(controller_command_task, "controller_commands", 6144U, nullptr,
                5U, nullptr);
}

}  // namespace firmware::target
