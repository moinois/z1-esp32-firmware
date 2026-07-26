// Verifies nonfatal web-volume mount and format-on-failure sequencing.
#include "test.hpp"

#include "firmware/application/web_volume_startup.hpp"

using firmware::application::WebVolumePort;
using firmware::application::WebVolumeStartup;

namespace {

class FakeWebVolumePort final : public WebVolumePort {
public:
    bool mount(const firmware::application::WebVolumeConfig& config) override {
        mounted_path = config.mount_path;
        ++mount_calls;
        return mount_result;
    }

    bool format(const firmware::application::WebVolumeConfig& config) override {
        formatted_path = config.mount_path;
        ++format_calls;
        return format_result;
    }

    bool mount_result = true;
    bool format_result = true;
    std::string_view mounted_path{};
    std::string_view formatted_path{};
    std::size_t mount_calls = 0U;
    std::size_t format_calls = 0U;
};

}  // namespace

TEST_CASE(web_002_mount_success_does_not_format) {
    FakeWebVolumePort port;
    WebVolumeStartup startup;
    REQUIRE(startup.start(port));
    REQUIRE_EQ(port.mount_calls, 1U);
    REQUIRE_EQ(port.format_calls, 0U);
}

TEST_CASE(web_002_mount_failure_formats_then_retries_once) {
    FakeWebVolumePort port;
    port.mount_result = false;
    WebVolumeStartup startup;
    REQUIRE(!startup.start(port));
    REQUIRE_EQ(port.mount_calls, 2U);
    REQUIRE_EQ(port.format_calls, 1U);

    port.mount_result = true;
    REQUIRE(startup.start(port));
    REQUIRE_EQ(port.mount_calls, 3U);
    REQUIRE_EQ(port.format_calls, 1U);
}
