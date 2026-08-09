// Verifies the target-neutral camera electrical and capture configuration.
#include "test.hpp"

#include "application/camera/camera_hardware_config.hpp"

using firmware::application::camera_hardware;

TEST_CASE(hw_040_camera_uses_exact_parallel_and_sccb_pins) {
    REQUIRE_EQ(camera_hardware.data_gpio[0], 11);
    REQUIRE_EQ(camera_hardware.data_gpio[1], 9);
    REQUIRE_EQ(camera_hardware.data_gpio[2], 8);
    REQUIRE_EQ(camera_hardware.data_gpio[3], 10);
    REQUIRE_EQ(camera_hardware.data_gpio[4], 12);
    REQUIRE_EQ(camera_hardware.data_gpio[5], 18);
    REQUIRE_EQ(camera_hardware.data_gpio[6], 17);
    REQUIRE_EQ(camera_hardware.data_gpio[7], 16);
    REQUIRE_EQ(camera_hardware.vsync_gpio, 6);
    REQUIRE_EQ(camera_hardware.href_gpio, 7);
    REQUIRE_EQ(camera_hardware.pixel_clock_gpio, 13);
    REQUIRE_EQ(camera_hardware.sccb_data_gpio, 4);
    REQUIRE_EQ(camera_hardware.sccb_clock_gpio, 5);
}

TEST_CASE(hw_041_camera_uses_twenty_mhz_xclk_without_control_pins) {
    REQUIRE_EQ(camera_hardware.external_clock_gpio, 15);
    REQUIRE_EQ(camera_hardware.external_clock_hz, 20000000U);
    REQUIRE_EQ(camera_hardware.power_down_gpio, -1);
    REQUIRE_EQ(camera_hardware.reset_gpio, -1);
}

TEST_CASE(hw_042_and_043_camera_capture_and_sensor_policy_is_exact) {
    REQUIRE(camera_hardware.jpeg_output);
    REQUIRE_EQ(camera_hardware.jpeg_quality, 15U);
    REQUIRE_EQ(camera_hardware.frame_buffer_count, 3U);
    REQUIRE(camera_hardware.frame_buffers_in_psram);
    REQUIRE(camera_hardware.capture_when_empty);
    REQUIRE_EQ(camera_hardware.startup_allocation_frame_size, 15U);
    REQUIRE(!camera_hardware.horizontal_mirror);
    REQUIRE(camera_hardware.vertical_flip);
    REQUIRE(camera_hardware.apply_stream_size_after_detection);
    REQUIRE_EQ(camera_hardware.capture_timeout_milliseconds, 4000U);
}
