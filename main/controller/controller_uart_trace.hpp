/** @file @brief Declares the opt-in asynchronous controller UART byte trace. */
#pragma once

#include "core/protocol/bytes.hpp"

#include <cstdint>

namespace firmware::target {

enum class ControllerUartTraceDirection : std::uint8_t {
    receive = 0U,
    transmit = 1U,
};

/// Starts the bounded trace consumer. Safe to call more than once.
void start_controller_uart_trace();

/// Copies one UART chunk into the nonblocking trace queue when available.
void trace_controller_uart(ControllerUartTraceDirection direction,
                           core::BytesView bytes);

/// Closes the trace before removable storage is unmounted.
void controller_uart_trace_storage_unmounted();

}  // namespace firmware::target
