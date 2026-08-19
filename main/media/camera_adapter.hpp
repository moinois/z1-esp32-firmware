/** @file @brief Declares the ESP-IDF DVP camera lifecycle and resolution adapter. */
#pragma once

#include "camera_hardware_adapter.hpp"

namespace firmware::target {

/// Owns the physical camera driver while exposing the portable resolution port.
class CameraAdapter final : public CameraHardwareAdapter {
public:
    /// Reserves the late camera startup's DMA memory before other services fragment it.
    CameraAdapter();

    /// Initializes the configured JPEG DVP camera and sensor orientation.
    bool initialize() override;

    /// Releases the camera driver before an application OTA update.
    bool deinitialize() override;

    /// Applies one logical frame-size dimension pair to the active sensor.
    bool set_frame_dimensions(
        firmware::application::FrameDimensions dimensions) override;

    /// Captures one JPEG frame using the driver's configured buffer timeout.
    std::optional<firmware::core::ByteVector> capture_jpeg() override;

    /// Returns the configured startup dimensions used by the recording writer.
    firmware::application::FrameDimensions current_dimensions() const override;

    /// Returns the cached normalized settings loaded during initialization.
    const firmware::application::CameraSettings& settings() const override;

private:
    void* dma_reservation_ = nullptr;
    bool initialized_ = false;
};

}  // namespace firmware::target
