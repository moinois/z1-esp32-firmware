// Declares the ESP-IDF DVP camera lifecycle and resolution adapter.
#pragma once

#include "firmware/application/camera_resolution_endpoint.hpp"
#include "firmware/application/camera_settings.hpp"
#include "firmware/core/bytes.hpp"

#include <optional>

namespace firmware::target {

// Owns the physical camera driver while exposing the portable resolution port.
class CameraAdapter final : public firmware::application::CameraResolutionPort {
public:
    // Initializes the configured JPEG DVP camera and sensor orientation.
    bool initialize();

    // Releases the camera driver before an application OTA update.
    bool deinitialize();

    // Applies one logical frame-size dimension pair to the active sensor.
    bool set_frame_dimensions(
        firmware::application::FrameDimensions dimensions) override;

    // Captures one JPEG frame using the driver's configured buffer timeout.
    std::optional<firmware::core::ByteVector> capture_jpeg();

    // Returns the configured startup dimensions used by the recording writer.
    firmware::application::FrameDimensions current_dimensions() const;

    // Returns the cached normalized settings loaded during initialization.
    const firmware::application::CameraSettings& settings() const;
};

// Returns the process-wide camera adapter used by HTTP and update services.
CameraAdapter& camera_adapter();

}  // namespace firmware::target
