// Verifies the camera-resolution HTTP endpoint through a replaceable sensor port.
#include "test.hpp"

#include "application/camera/camera_resolution_endpoint.hpp"

#include <string_view>

using firmware::application::CameraResolutionEndpoint;
using firmware::application::CameraResolutionPort;
using firmware::application::FrameDimensions;

namespace {

class FakeCameraResolutionPort final : public CameraResolutionPort {
public:
    bool set_frame_dimensions(FrameDimensions dimensions) override {
        last_dimensions = dimensions;
        ++calls;
        return accepts;
    }

    bool accepts = true;
    FrameDimensions last_dimensions{};
    std::size_t calls = 0U;
};

}  // namespace

TEST_CASE(web_020_empty_or_invalid_resolution_body_returns_exact_500) {
    FakeCameraResolutionPort port;
    CameraResolutionEndpoint endpoint;
    const auto empty = endpoint.handle(firmware::core::BytesView{}, port);
    REQUIRE_EQ(empty.status_code, 500U);
    REQUIRE_EQ(empty.body, std::string_view("No Data"));

    const auto invalid = endpoint.handle("not-json", port);
    REQUIRE_EQ(invalid.status_code, 500U);
    REQUIRE_EQ(invalid.body, std::string_view("Invalid JSON"));
    REQUIRE_EQ(port.calls, 0U);
}

TEST_CASE(web_021_missing_or_nonnumeric_resolution_returns_exact_500) {
    FakeCameraResolutionPort port;
    CameraResolutionEndpoint endpoint;
    const auto missing = endpoint.handle("{}", port);
    REQUIRE_EQ(missing.status_code, 500U);
    REQUIRE_EQ(missing.body, std::string_view("Missing or invalid 'resolution'"));

    const auto wrong_type = endpoint.handle("{\"resolution\":\"7\"}", port);
    REQUIRE_EQ(wrong_type.status_code, 500U);
    REQUIRE_EQ(wrong_type.body, std::string_view("Missing or invalid 'resolution'"));
}

TEST_CASE(web_022_resolution_is_normalized_and_sensor_failures_are_500) {
    FakeCameraResolutionPort port;
    CameraResolutionEndpoint endpoint;
    const auto normalized = endpoint.handle("{\"resolution\":0}", port);
    REQUIRE_EQ(normalized.status_code, 200U);
    REQUIRE_EQ(port.last_dimensions, FrameDimensions({640U, 480U}));

    port.accepts = false;
    const auto failed = endpoint.handle("{\"resolution\":1}", port);
    REQUIRE_EQ(failed.status_code, 500U);
    REQUIRE_EQ(failed.body, std::string_view("Failed to set framesize"));
}

TEST_CASE(web_020_endpoint_reads_at_most_63_request_body_bytes) {
    FakeCameraResolutionPort port;
    CameraResolutionEndpoint endpoint;
    const auto response = endpoint.handle(
        "{\"resolution\":15} trailing bytes that exceed the endpoint body limit", port);
    REQUIRE_EQ(response.status_code, 200U);
    REQUIRE_EQ(port.last_dimensions, FrameDimensions({1600U, 1200U}));
}
