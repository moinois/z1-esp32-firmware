// Defines the controller UART contract shared by policy tests and the ESP-IDF adapter.
#pragma once

#include <cstddef>
#include <cstdint>

namespace firmware::application {

// Groups the immutable hardware and receive-loop settings for the controller link.
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

constexpr ControllerUartConfig controller_uart{
    1,
    42,
    41,
    -1,
    -1,
    230400,
    8,
    1,
    false,
    false,
    true,
    1024U,
    1024U,
    512U,
    30U,
};

}  // namespace firmware::application
