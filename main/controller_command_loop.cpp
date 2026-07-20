// Implements UART frame decoding and wall-clock command dispatch.
#include "controller_command_loop.hpp"

#include "controller_uart_adapter.hpp"
#include "wall_clock_adapter.hpp"
#include "wall_clock_command_dispatcher.hpp"
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
    firmware::core::StreamDecoder decoder(
        firmware::core::StreamPolicy::controller_uart());
    std::uint8_t input[256];
    for (;;) {
        const int count = uart.read(input, sizeof(input));
        if (count <= 0) continue;
        const auto frames = decoder.push(
            {input, static_cast<std::size_t>(count)});
        for (const auto& frame : frames) dispatcher.dispatch(frame);
    }
}

}  // namespace

void ControllerCommandLoop::start() {
    xTaskCreate(controller_command_task, "controller_commands", 6144U, nullptr,
                5U, nullptr);
}

}  // namespace firmware::target
