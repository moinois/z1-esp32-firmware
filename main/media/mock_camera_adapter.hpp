/** @file @brief Declares a deterministic camera implementation for camera-less HIL targets. */
#pragma once

#include "camera_hardware_adapter.hpp"

namespace firmware::target {

/// Supplies deterministic JPEG frames without probing physical camera pins.
class MockCameraAdapter final : public CameraHardwareAdapter {
public:
    /// Activates the volatile simulated camera.
    bool initialize() override;

    /// Stops simulated capture without touching hardware.
    bool deinitialize() override;

    /// Applies any dimensions from the normative camera table.
    bool set_frame_dimensions(
        firmware::application::FrameDimensions dimensions) override;

    /// Returns one deterministic JPEG marker stream while initialized.
    std::optional<firmware::core::ByteVector> capture_jpeg() override;

    /// Returns the most recently selected dimensions.
    firmware::application::FrameDimensions current_dimensions() const override;

    /// Returns settings loaded through the normal SD configuration path.
    const firmware::application::CameraSettings& settings() const override;

private:
    bool initialized_ = false;
    firmware::application::FrameDimensions dimensions_ =
        firmware::application::fallback_camera_dimensions;
};

}  // namespace firmware::target
