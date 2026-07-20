// Declares the ESP-IDF DVP camera lifecycle and resolution adapter.
#pragma once

#include "firmware/application/camera_resolution_endpoint.hpp"

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
};

// Returns the process-wide camera adapter used by HTTP and update services.
CameraAdapter& camera_adapter();

}  // namespace firmware::target
