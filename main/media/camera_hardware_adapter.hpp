/** @file @brief Declares the common lifecycle and capture surface for live and mock cameras. */
#pragma once

#include "application/camera/camera_resolution_endpoint.hpp"
#include "application/camera/camera_settings.hpp"
#include "core/protocol/bytes.hpp"

#include <optional>

namespace firmware::target {

/// Keeps application and transport code independent of the selected camera.
class CameraHardwareAdapter
    : public firmware::application::CameraResolutionPort {
public:
    /// Enables safe destruction through either hardware implementation.
    ~CameraHardwareAdapter() override = default;

    /// Starts the selected camera and loads its normalized settings.
    virtual bool initialize() = 0;

    /// Stops capture before an application OTA update.
    virtual bool deinitialize() = 0;

    /// Captures one complete JPEG frame when the camera is available.
    virtual std::optional<firmware::core::ByteVector> capture_jpeg() = 0;

    /// Returns the dimensions currently selected by the adapter.
    virtual firmware::application::FrameDimensions current_dimensions() const = 0;

    /// Returns normalized settings shared by streaming and recording.
    virtual const firmware::application::CameraSettings& settings() const = 0;
};

}  // namespace firmware::target
