/** @file @brief Defines the controller UART contract shared by policy tests and the ESP-IDF adapter. */
#pragma once

#include <cstddef>
#include <cstdint>

namespace firmware::application {

/// Groups the immutable hardware and receive-loop settings for the controller link.
struct ControllerUartConfig {
    int port;
    int tx_gpio;
    int rx_gpio;
    int rts_gpio;
    int cts_gpio;
    int baud_rate;
    int data_bits;
    int stop_bits;
    bool parity_enabled;
    bool flow_control_enabled;
    bool use_apb_clock;
    std::size_t receive_buffer_size;
    std::size_t transmit_buffer_size;
    std::size_t maximum_read_size;
    std::uint32_t read_wait_milliseconds;
};

inline constexpr int controller_uart_port = 1;
inline constexpr int controller_uart_tx_gpio = 42;
inline constexpr int controller_uart_rx_gpio = 41;
inline constexpr int controller_uart_unused_gpio = -1;
inline constexpr int controller_uart_baud_rate = 230400;
inline constexpr int controller_uart_data_bits = 8;
inline constexpr int controller_uart_stop_bits = 1;
inline constexpr std::size_t controller_uart_receive_buffer_size = 2048U;
inline constexpr std::size_t controller_uart_transmit_buffer_size = 1024U;
inline constexpr std::size_t controller_uart_maximum_read_size = 512U;
inline constexpr std::uint32_t controller_uart_read_wait_milliseconds = 30U;

constexpr ControllerUartConfig controller_uart{
    controller_uart_port,
    controller_uart_tx_gpio,
    controller_uart_rx_gpio,
    controller_uart_unused_gpio,
    controller_uart_unused_gpio,
    controller_uart_baud_rate,
    controller_uart_data_bits,
    controller_uart_stop_bits,
    false,
    false,
    true,
    controller_uart_receive_buffer_size,
    controller_uart_transmit_buffer_size,
    controller_uart_maximum_read_size,
    controller_uart_read_wait_milliseconds,
};

}  // namespace firmware::application
