// Implements direct application OTA ordering, abort behavior, and restart handoff.
#include "firmware/application/direct_application_update.hpp"

namespace firmware::application {
namespace {

constexpr std::uint32_t restart_delay_milliseconds = 2000U;
constexpr std::string_view no_partition_body = "No valid OTA partition detected";
constexpr std::string_view erase_failure_body = "OTA partition wipe unsuccessful";
constexpr std::string_view begin_failure_body = "Unable to initialize OTA process";
constexpr std::string_view finish_failure_body = "OTA finish Failed";
constexpr std::string_view boot_failure_body = "Failed to set boot partition";
constexpr std::string_view success_body =
    "Firmware upgrade finished. The system will reboot in 2 seconds...";

}  // namespace

bool DirectApplicationUpdateService::apply(
    core::BytesView image, DirectApplicationUpdatePort& port) const {
    if (!port.deinitialize_camera()) {
        return fail(port, finish_failure_body);
    }
    if (image.size() == 0U) {
        return fail(port, finish_failure_body);
    }
    if (!port.select_inactive_partition()) {
        return fail(port, no_partition_body);
    }
    if (!port.erase_partition()) {
        return fail(port, erase_failure_body);
    }
    if (!port.begin_update(image.size())) {
        return fail(port, begin_failure_body);
    }
    if (!port.write_image(image)) {
        port.abort_update();
        return fail(port, finish_failure_body);
    }
    if (!port.finish_update()) {
        port.abort_update();
        return fail(port, finish_failure_body);
    }
    if (!port.select_boot_partition()) {
        return fail(port, boot_failure_body);
    }
    port.send_response(200U, success_body);
    port.delay_milliseconds(restart_delay_milliseconds);
    port.restart();
    return true;
}

bool DirectApplicationUpdateService::fail(DirectApplicationUpdatePort& port,
                                          std::string_view body) {
    port.send_response(500U, body);
    return false;
}

}  // namespace firmware::application
