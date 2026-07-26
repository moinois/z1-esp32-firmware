// Verifies controller UART activity monitoring and bounded output scheduling.
#include "test.hpp"

#include "firmware/application/controller_link.hpp"

using firmware::application::ControllerActivityMonitor;
using firmware::application::ControllerOutputQueue;

TEST_CASE(uart_003_valid_frames_reset_the_activity_deadline) {
    ControllerActivityMonitor monitor(1000U);

    REQUIRE(monitor.poll(10999U).empty());
    monitor.record_valid_frame(10999U);
    REQUIRE(monitor.poll(20998U).empty());
    REQUIRE_EQ(monitor.poll(20999U).size(), 1U);
}

TEST_CASE(uart_004_inactivity_creates_the_exact_console_alarm) {
    ControllerActivityMonitor monitor(0U);

    const auto alarms = monitor.poll(10000U);

    REQUIRE_EQ(alarms.size(), 1U);
    REQUIRE_EQ(alarms.front().type, 0x90U);
    REQUIRE_EQ(alarms.front().payload,
               firmware::core::ByteVector({
                   'A', 'L', 'A', 'R', 'M', ':', ' ', 'M', 'a', 'i', 'n', 'b', 'o', 'a', 'r', 'd',
                   ' ', 'd', 'i', 'd', ' ', 'n', 'o', 't', ' ', 'r', 'e', 'c', 'e', 'i', 'v', 'e',
                   ' ', 'a', ' ', 's', 't', 'a', 't', 'u', 's', ' ', 'r', 'e', 's', 'p', 'o', 'n',
                   's', 'e', ' ', 'f', 'r', 'o', 'm', ' ', 't', 'h', 'e', ' ', 'C', 'T', 'R', 'L',
                   ' ', '(', 'R', 'X', ' ', 'e', 'r', 'r', 'o', 'r', ')', '\n',
               }));
}

TEST_CASE(uart_004_each_elapsed_inactivity_period_creates_an_alarm) {
    ControllerActivityMonitor monitor(0U);

    REQUIRE_EQ(monitor.poll(30000U).size(), 3U);
    REQUIRE(monitor.poll(39999U).empty());
    REQUIRE_EQ(monitor.poll(40000U).size(), 1U);
}

TEST_CASE(uart_005_output_is_fifo_and_rejects_the_thirty_third_item) {
    ControllerOutputQueue output;

    for (std::uint8_t value = 0U; value < 32U; ++value) {
        REQUIRE(output.enqueue({value}));
    }
    REQUIRE(!output.enqueue({32U}));
    REQUIRE_EQ(output.pending(), 32U);
    REQUIRE_EQ(output.take_ready(0U).value(), firmware::core::ByteVector({0U}));
    REQUIRE_EQ(output.take_ready(10U).value(), firmware::core::ByteVector({1U}));
}

TEST_CASE(uart_006_output_rejects_empty_and_oversize_items) {
    ControllerOutputQueue output;

    REQUIRE(!output.enqueue({}));
    REQUIRE(output.enqueue(firmware::core::ByteVector(544U, 0xAAU)));
    REQUIRE(!output.enqueue(firmware::core::ByteVector(545U, 0xAAU)));
}

TEST_CASE(uart_007_output_waits_ten_milliseconds_after_each_write_start) {
    ControllerOutputQueue output;
    REQUIRE(output.enqueue({1U}));
    REQUIRE(output.enqueue({2U}));

    REQUIRE(output.take_ready(25U).has_value());
    REQUIRE(!output.take_ready(34U).has_value());
    REQUIRE(output.take_ready(35U).has_value());
}

TEST_CASE(uart_008_negative_write_result_requests_the_exact_diagnostic) {
    ControllerOutputQueue output;

    const auto diagnostic = output.record_write_result(-1);

    REQUIRE(diagnostic.has_value());
    REQUIRE_EQ(diagnostic->tag, std::string_view("uart_task"));
    REQUIRE_EQ(diagnostic->message, std::string_view("UART send failed"));
}

TEST_CASE(uart_008_short_positive_write_is_not_retried) {
    ControllerOutputQueue output;
    REQUIRE(output.enqueue({1U, 2U, 3U}));

    const auto item = output.take_ready(0U);

    REQUIRE(item.has_value());
    REQUIRE(!output.record_write_result(1).has_value());
    REQUIRE_EQ(output.pending(), 0U);
}
