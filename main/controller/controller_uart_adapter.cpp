// Implements controller UART access using the ESP-IDF UART driver.
#include "controller_uart_adapter.hpp"

#include "firmware/application/controller_uart_config.hpp"

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"

#include <algorithm>

namespace firmware::target {
namespace {

constexpr uart_port_t uart_port = static_cast<uart_port_t>(
    application::controller_uart.port);
constexpr std::uint8_t disabled_rx_flow_threshold = 0U;
constexpr int unused_event_queue_size = 0;
constexpr int default_interrupt_flags = 0;

}  // namespace

bool ControllerUartAdapter::initialize() {
    const uart_config_t config{
        .baud_rate = application::controller_uart.baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = disabled_rx_flow_threshold,
        .source_clk = UART_SCLK_APB,
        .flags = {},
    };

    if (uart_param_config(uart_port, &config) != ESP_OK) {
        return false;
    }
    if (uart_set_pin(uart_port,
                     application::controller_uart.tx_gpio,
                     application::controller_uart.rx_gpio,
                     application::controller_uart.rts_gpio,
                     application::controller_uart.cts_gpio) != ESP_OK) {
        return false;
    }
    return uart_driver_install(uart_port,
                               application::controller_uart.receive_buffer_size,
                               application::controller_uart.transmit_buffer_size,
                               unused_event_queue_size,
                               nullptr,
                               default_interrupt_flags) == ESP_OK;
}

int ControllerUartAdapter::read(std::uint8_t* destination, std::size_t capacity) {
    const std::size_t read_size =
        std::min(capacity, application::controller_uart.maximum_read_size);
    return uart_read_bytes(uart_port,
                           destination,
                           read_size,
                           pdMS_TO_TICKS(application::controller_uart.read_wait_milliseconds));
}

int ControllerUartAdapter::write(core::BytesView frame) {
    return uart_write_bytes(uart_port, frame.data(), frame.size());
}

}  // namespace firmware::target
