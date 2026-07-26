// Implements raw web-volume update ordering and empty-image success behavior.
#include "firmware/application/direct_web_volume_update.hpp"

namespace firmware::application {
namespace {

constexpr std::uint32_t restart_delay_milliseconds = 2000U;
constexpr std::string_view missing_partition_body = "SPIFFS partition not found";
constexpr std::string_view erase_failure_body = "SPIFFS erase failed";
constexpr std::string_view success_body =
    "UI upgrade finished. The system will reboot in 2 seconds...";

}  // namespace

bool DirectWebVolumeUpdateService::apply(
    core::BytesView content, DirectWebVolumeUpdatePort& port) const {
    const auto capacity = port.partition_size();
    if (!capacity.has_value()) {
        return fail(port, missing_partition_body);
    }
    if (content.size() > *capacity) {
        return false;
    }
    if (!port.erase_partition()) {
        return fail(port, erase_failure_body);
    }
    if (!port.write_content(content)) {
        return false;
    }
    port.send_response(200U, success_body);
    port.delay_milliseconds(restart_delay_milliseconds);
    port.restart();
    return true;
}

bool DirectWebVolumeUpdateService::fail(DirectWebVolumeUpdatePort& port,
                                         std::string_view body) {
    port.send_response(500U, body);
    return false;
}

}  // namespace firmware::application
