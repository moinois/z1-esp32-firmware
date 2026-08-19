/** @file @brief Implements the ESP-IDF camera pin, capture, orientation, and resolution bridge. */
#include "camera_adapter.hpp"

#include "esp_camera.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "application/camera/camera_hardware_config.hpp"
#include "application/camera/camera_settings.hpp"
#include "camera_settings_adapter.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace firmware::target {
namespace {

constexpr char tag[] = "CAMERA";
// LIVE-010 delays driver startup until the first video client. Hold one early
// internal DMA block so unrelated service allocation cannot fragment every
// block large enough for the camera driver before that normative trigger.
constexpr std::size_t late_camera_dma_reservation_size = 16U * 1024U;
// Maps the portable CAM-001 dimension table to esp32-camera frame-size values.
constexpr std::array<framesize_t, 15U> frame_sizes{
    FRAMESIZE_QQVGA, FRAMESIZE_128X128, FRAMESIZE_QCIF, FRAMESIZE_HQVGA,
    FRAMESIZE_240X240, FRAMESIZE_QVGA, FRAMESIZE_320X320, FRAMESIZE_CIF,
    FRAMESIZE_HVGA, FRAMESIZE_VGA, FRAMESIZE_SVGA, FRAMESIZE_XGA,
    FRAMESIZE_HD, FRAMESIZE_SXGA, FRAMESIZE_UXGA};

// Converts the immutable hardware policy to the ESP-IDF camera structure.
camera_config_t make_camera_config() {
    const auto& policy = firmware::application::camera_hardware;
    return camera_config_t{
        .pin_pwdn = policy.power_down_gpio,
        .pin_reset = policy.reset_gpio,
        .pin_xclk = policy.external_clock_gpio,
        .pin_sccb_sda = policy.sccb_data_gpio,
        .pin_sccb_scl = policy.sccb_clock_gpio,
        .pin_d7 = policy.data_gpio[7],
        .pin_d6 = policy.data_gpio[6],
        .pin_d5 = policy.data_gpio[5],
        .pin_d4 = policy.data_gpio[4],
        .pin_d3 = policy.data_gpio[3],
        .pin_d2 = policy.data_gpio[2],
        .pin_d1 = policy.data_gpio[1],
        .pin_d0 = policy.data_gpio[0],
        .pin_vsync = policy.vsync_gpio,
        .pin_href = policy.href_gpio,
        .pin_pclk = policy.pixel_clock_gpio,
        .xclk_freq_hz = static_cast<int>(policy.external_clock_hz),
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = frame_sizes[policy.startup_allocation_frame_size - 1U],
        .jpeg_quality = policy.jpeg_quality,
        .fb_count = policy.frame_buffer_count,
        .fb_location = policy.frame_buffers_in_psram
                          ? CAMERA_FB_IN_PSRAM
                          : CAMERA_FB_IN_DRAM,
        .grab_mode = policy.capture_when_empty
                         ? CAMERA_GRAB_WHEN_EMPTY
                         : CAMERA_GRAB_LATEST,
        .sccb_i2c_port = 0,
    };
}

}  // namespace

CameraAdapter::CameraAdapter()
    : dma_reservation_(heap_caps_malloc(late_camera_dma_reservation_size,
                                        MALLOC_CAP_DMA)) {
    if (dma_reservation_ == nullptr) {
        ESP_LOGW(tag, "Could not reserve late camera DMA memory");
    }
}

bool CameraAdapter::initialize() {
    if (initialized_) {
        return true;
    }
    // Release immediately before esp_camera_init so the driver receives the
    // contiguous block without initializing the sensor before LIVE-010 allows.
    heap_caps_free(dma_reservation_);
    dma_reservation_ = nullptr;
    const auto& camera_settings = load_camera_settings();
    const auto config = make_camera_config();
    const esp_err_t result = esp_camera_init(&config);
    if (result != ESP_OK) {
        initialized_ = false;
        ESP_LOGW(tag, "Camera initialization failed: %s", esp_err_to_name(result));
        return false;
    }
    sensor_t* sensor = esp_camera_sensor_get();
    if (sensor == nullptr) {
        ESP_LOGW(tag, "Camera sensor was not detected after initialization");
        static_cast<void>(esp_camera_deinit());
        initialized_ = false;
        return false;
    }
    sensor->set_hmirror(sensor, firmware::application::camera_hardware.horizontal_mirror);
    sensor->set_vflip(sensor, firmware::application::camera_hardware.vertical_flip);
    if (sensor->set_framesize(sensor, frame_sizes[
            camera_settings.stream_frame_size - 1U]) != 0) {
        ESP_LOGW(tag, "Configured stream frame size could not be applied");
    }
    initialized_ = true;
    return true;
}

bool CameraAdapter::deinitialize() {
    // OTA may be requested when camera probing failed during boot. That state
    // is already safely deinitialized and must not prevent an update.
    if (!initialized_) {
        return true;
    }
    if (esp_camera_deinit() != ESP_OK) {
        return false;
    }
    initialized_ = false;
    return true;
}

bool CameraAdapter::set_frame_dimensions(
    firmware::application::FrameDimensions dimensions) {
    for (std::size_t index = 1U; index <= frame_sizes.size(); ++index) {
        if (firmware::application::camera_dimensions(
                static_cast<std::uint8_t>(index)) == dimensions) {
            sensor_t* sensor = esp_camera_sensor_get();
            return sensor != nullptr &&
                   sensor->set_framesize(sensor, frame_sizes[index - 1U]) == 0;
        }
    }
    return false;
}

std::optional<firmware::core::ByteVector> CameraAdapter::capture_jpeg() {
    camera_fb_t* frame = esp_camera_fb_get();
    if (frame == nullptr) {
        ESP_LOGW(tag, "Camera capture returned no frame");
        return std::nullopt;
    }
    if (frame->format != PIXFORMAT_JPEG) {
        ESP_LOGW(tag, "Camera capture returned unexpected format: %d",
                 static_cast<int>(frame->format));
        esp_camera_fb_return(frame);
        return std::nullopt;
    }
    firmware::core::ByteVector jpeg(frame->buf, frame->buf + frame->len);
    esp_camera_fb_return(frame);
    return jpeg;
}

firmware::application::FrameDimensions CameraAdapter::current_dimensions() const {
    const sensor_t* sensor = esp_camera_sensor_get();
    if (sensor == nullptr) {
        return firmware::application::fallback_camera_dimensions;
    }
    // ESP-IDF frame-size values are zero-based; the product table is one-based.
    const std::size_t table_index =
        static_cast<std::size_t>(sensor->status.framesize) + 1U;
    if (table_index > frame_sizes.size()) {
        return firmware::application::fallback_camera_dimensions;
    }
    return firmware::application::camera_dimensions(
        static_cast<std::uint8_t>(table_index));
}

const firmware::application::CameraSettings& CameraAdapter::settings() const {
    return load_camera_settings();
}

}  // namespace firmware::target
