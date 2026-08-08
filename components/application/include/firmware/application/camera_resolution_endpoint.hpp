/** @file @brief Declares the bounded camera-resolution HTTP endpoint policy. */
#pragma once

#include "firmware/application/camera_settings.hpp"
#include "firmware/core/bytes.hpp"
#include "firmware/core/http_policy.hpp"

#include <string_view>

namespace firmware::application {

/// Isolates frame-size application from the camera driver and HTTP transport.
class CameraResolutionPort {
public:
    /// Enables safe destruction through a substituted camera implementation.
    virtual ~CameraResolutionPort() = default;

    /// Applies dimensions to the active sensor and reports driver acceptance.
    virtual bool set_frame_dimensions(FrameDimensions dimensions) = 0;
};

/// Parses one bounded request and maps sensor outcomes to exact HTTP responses.
class CameraResolutionEndpoint {
public:
    /// Handles only the first configured request-body bytes.
    core::HttpResponsePolicy handle(core::BytesView request_body,
                                    CameraResolutionPort& camera) const;

    /// Offers a convenient text view for host tests and transport-neutral callers.
    core::HttpResponsePolicy handle(std::string_view request_body,
                                    CameraResolutionPort& camera) const {
        return handle(core::BytesView(request_body), camera);
    }
};

}  // namespace firmware::application
