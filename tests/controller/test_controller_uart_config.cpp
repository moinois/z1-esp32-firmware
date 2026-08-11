// Verifies the controller UART hardware and receive-loop configuration.
#include "test.hpp"

#include "application/controller/controller_uart_config.hpp"

using firmware::application::controller_uart;

TEST_CASE(hw_020_controller_uart_uses_the_required_port_pins_and_line_rate) {
    REQUIRE_EQ(controller_uart.port, 1);
    REQUIRE_EQ(controller_uart.tx_gpio, 42);
    REQUIRE_EQ(controller_uart.rx_gpio, 41);
    REQUIRE_EQ(controller_uart.baud_rate, 230400);
    REQUIRE_EQ(controller_uart.data_bits, 8);
    REQUIRE_EQ(controller_uart.stop_bits, 1);
    REQUIRE(!controller_uart.parity_enabled);
    REQUIRE(!controller_uart.flow_control_enabled);
}

TEST_CASE(hw_021_controller_uart_uses_apb_and_has_no_flow_control_pins) {
    REQUIRE(controller_uart.use_apb_clock);
    REQUIRE_EQ(controller_uart.rts_gpio, -1);
    REQUIRE_EQ(controller_uart.cts_gpio, -1);
}

TEST_CASE(uart_009_controller_uart_can_retain_2048_undecoded_bytes) {
    REQUIRE_EQ(controller_uart.receive_buffer_size, 2048U);
    REQUIRE_EQ(controller_uart.transmit_buffer_size, 1024U);
}

TEST_CASE(uart_002_receive_loop_uses_bounded_reads_and_waits) {
    REQUIRE_EQ(controller_uart.maximum_read_size, 512U);
    REQUIRE_EQ(controller_uart.read_wait_milliseconds, 30U);
}
