/** @file @brief Defines the target-neutral camera pin, allocation, and capture contract. */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace firmware::application {

/// Groups all fixed electrical and capture settings for the camera adapter.
struct CameraHardwareConfig {
    std::array<int, 8U> data_gpio;
    int vsync_gpio;
    int href_gpio;
    int pixel_clock_gpio;
    int sccb_data_gpio;
    int sccb_clock_gpio;
    int external_clock_gpio;
    std::uint32_t external_clock_hz;
    int power_down_gpio;
    int reset_gpio;
    bool jpeg_output;
    std::uint8_t jpeg_quality;
    std::size_t frame_buffer_count;
    bool frame_buffers_in_psram;
    bool capture_when_empty;
    std::uint8_t startup_allocation_frame_size;
    bool horizontal_mirror;
    bool vertical_flip;
    bool apply_stream_size_after_detection;
    std::uint32_t capture_timeout_milliseconds;
};

inline constexpr CameraHardwareConfig camera_hardware{
    {11, 9, 8, 10, 12, 18, 17, 16},
    6,
    7,
    13,
    4,
    5,
    15,
    20000000U,
    -1,
    -1,
    true,
    15U,
    3U,
    true,
    true,
    15U,
    false,
    true,
    true,
    4000U,
};

}  // namespace firmware::application
