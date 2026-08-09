/** @file @brief Defines the target-neutral TWAI hardware contract used by its ESP-IDF adapter. */
#pragma once

#include <cstddef>
#include <cstdint>

namespace firmware::application {

/// Groups all fixed CAN controller, timing, filter, queue, and pin settings.
struct CanTwaiConfig {
    bool normal_mode;
    std::uint32_t bitrate;
    int tx_gpio;
    int rx_gpio;
    std::uint32_t source_clock_hz;
    std::uint32_t baud_rate_prescaler;
    std::uint32_t time_quanta_hz;
    std::uint8_t time_segment_1;
    std::uint8_t time_segment_2;
    std::uint8_t synchronization_jump_width;
    bool triple_sampling;
    bool accept_all_identifiers;
    std::size_t transmit_queue_capacity;
    std::size_t receive_queue_capacity;
    int clkout_gpio;
    int bus_off_indicator_gpio;
    int transceiver_control_gpio;
};

inline constexpr CanTwaiConfig can_twai{
    true,
    1000000U,
    46,
    14,
    80000000U,
    4U,
    20000000U,
    15U,
    4U,
    3U,
    false,
    true,
    16U,
    128U,
    -1,
    -1,
    -1,
};

}  // namespace firmware::application
