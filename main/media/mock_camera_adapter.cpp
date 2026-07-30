// Implements deterministic camera behavior without physical camera access.
#include "mock_camera_adapter.hpp"

#include "camera_settings_adapter.hpp"

#include "esp_log.h"

#include <cstddef>
#include <cstdint>
#include <iterator>

namespace firmware::target {
namespace {

constexpr char tag[] = "MOCK_CAMERA";
constexpr std::uint8_t deterministic_jpeg[]{0xffU, 0xd8U, 0xffU, 0xd9U};

// Reports whether dimensions belong to the normative camera size table.
bool supported_dimensions(firmware::application::FrameDimensions dimensions) {
    for (std::size_t value = firmware::application::minimum_camera_frame_size;
         value <= firmware::application::maximum_camera_frame_size; ++value) {
        if (firmware::application::camera_dimensions(
                static_cast<std::uint8_t>(value)) == dimensions) {
            return true;
        }
    }
    return false;
}

}  // namespace

bool MockCameraAdapter::initialize() {
    dimensions_ = firmware::application::camera_dimensions(
        settings().stream_frame_size);
    initialized_ = true;
    ESP_LOGW(tag, "TEST BUILD: deterministic mock camera initialized");
    return true;
}

bool MockCameraAdapter::deinitialize() {
    initialized_ = false;
    return true;
}

bool MockCameraAdapter::set_frame_dimensions(
    firmware::application::FrameDimensions dimensions) {
    if (!initialized_ || !supported_dimensions(dimensions)) {
        return false;
    }
    dimensions_ = dimensions;
    return true;
}

std::optional<firmware::core::ByteVector> MockCameraAdapter::capture_jpeg() {
    if (!initialized_) {
        return std::nullopt;
    }
    return firmware::core::ByteVector(
        std::begin(deterministic_jpeg), std::end(deterministic_jpeg));
}

firmware::application::FrameDimensions
MockCameraAdapter::current_dimensions() const {
    return dimensions_;
}

const firmware::application::CameraSettings& MockCameraAdapter::settings() const {
    return load_camera_settings();
}

}  // namespace firmware::target
