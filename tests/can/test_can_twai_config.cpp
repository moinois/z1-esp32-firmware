// Verifies the portable contract used by the ESP-IDF TWAI adapter.
#include "test.hpp"

#include "application/can/can_twai_config.hpp"

using firmware::application::can_twai;

TEST_CASE(hw_050_can_uses_normal_one_megabit_mode_and_exact_pins) {
    REQUIRE(can_twai.normal_mode);
    REQUIRE_EQ(can_twai.bitrate, 1000000U);
    REQUIRE_EQ(can_twai.tx_gpio, 46);
    REQUIRE_EQ(can_twai.rx_gpio, 14);
}

TEST_CASE(hw_051_can_timing_uses_twenty_mhz_quanta_and_single_sampling) {
    REQUIRE_EQ(can_twai.source_clock_hz, 80000000U);
    REQUIRE_EQ(can_twai.baud_rate_prescaler, 4U);
    REQUIRE_EQ(can_twai.time_quanta_hz, 20000000U);
    REQUIRE_EQ(can_twai.time_segment_1, 15U);
    REQUIRE_EQ(can_twai.time_segment_2, 4U);
    REQUIRE_EQ(can_twai.synchronization_jump_width, 3U);
    REQUIRE(!can_twai.triple_sampling);
}

TEST_CASE(hw_052_can_accepts_all_ids_with_exact_queue_capacities) {
    REQUIRE(can_twai.accept_all_identifiers);
    REQUIRE_EQ(can_twai.transmit_queue_capacity, 16U);
    REQUIRE_EQ(can_twai.receive_queue_capacity, 128U);
    REQUIRE_EQ(can_twai.clkout_gpio, -1);
    REQUIRE_EQ(can_twai.bus_off_indicator_gpio, -1);
    REQUIRE_EQ(can_twai.transceiver_control_gpio, -1);
}
