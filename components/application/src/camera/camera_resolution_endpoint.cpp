/** @file @brief Implements camera-resolution JSON validation, normalization, and responses. */
#include "application/camera/camera_resolution_endpoint.hpp"

#include "core/configuration/json_input.hpp"

#include <cmath>
#include <cstdint>
#include <limits>

namespace firmware::application {
namespace {

constexpr std::size_t maximum_request_body_bytes = 63U;
constexpr std::string_view no_data_body = "No Data";
constexpr std::string_view invalid_json_body = "Invalid JSON";
constexpr std::string_view invalid_resolution_body =
    "Missing or invalid 'resolution'";
constexpr std::string_view sensor_failure_body = "Failed to set framesize";
constexpr std::string_view success_body = "Resolution changed successfully";

// Creates a complete HTML error response with the specified status and body.
core::HttpResponsePolicy error_response(std::uint16_t status,
                                        std::string_view body) {
    return {status, "text/html", body, false, false};
}

// Converts a valid integer-like JSON number to the CAM-001 frame-size range.
std::uint8_t normalize_frame_size(double value) {
    const double truncated = std::trunc(value);
    const std::int32_t signed_value =
        truncated <= static_cast<double>(std::numeric_limits<std::int32_t>::min())
            ? std::numeric_limits<std::int32_t>::min()
        : truncated >= static_cast<double>(std::numeric_limits<std::int32_t>::max())
            ? std::numeric_limits<std::int32_t>::max()
            : static_cast<std::int32_t>(truncated);
    if (signed_value < minimum_camera_frame_size ||
        signed_value > maximum_camera_frame_size) {
        return default_camera_frame_size;
    }
    return static_cast<std::uint8_t>(signed_value);
}

}  // namespace

core::HttpResponsePolicy CameraResolutionEndpoint::handle(
    core::BytesView request_body, CameraResolutionPort& camera) const {
    if (request_body.size() == 0U) {
        return error_response(500U, no_data_body);
    }

    const core::BytesView bounded_body(request_body.data(),
                                       request_body.size() > maximum_request_body_bytes
                                           ? maximum_request_body_bytes
                                           : request_body.size());
    const auto document = core::parse_json_prefix(bounded_body);
    if (!document.has_value()) {
        return error_response(500U, invalid_json_body);
    }
    const auto resolution = core::find_json_number(*document, "resolution");
    if (!resolution.has_value()) {
        return error_response(500U, invalid_resolution_body);
    }

    const FrameDimensions dimensions = camera_dimensions(
        normalize_frame_size(*resolution));
    if (!camera.set_frame_dimensions(dimensions)) {
        return error_response(400U, sensor_failure_body);
    }
    return {200U, "text/html", success_body, false, false};
}

}  // namespace firmware::application
