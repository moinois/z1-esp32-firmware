/** @file @brief Implements raw web-volume update ordering and empty-image success behavior. */
#include "application/update/direct_web_volume_update.hpp"

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
    DirectWebVolumeUpdateService transaction;
    if (!transaction.begin(port)) return false;
    transaction.offer(content, port);
    return transaction.finish(port);
}

bool DirectWebVolumeUpdateService::begin(DirectWebVolumeUpdatePort& port) {
    const auto capacity = port.partition_size();
    if (!capacity.has_value()) {
        return fail(port, missing_partition_body);
    }
    if (!port.erase_partition()) {
        return fail(port, erase_failure_body);
    }
    offset_ = 0U;
    return true;
}

void DirectWebVolumeUpdateService::offer(
    core::BytesView content, DirectWebVolumeUpdatePort& port) {
    static_cast<void>(port.write_content(offset_, content));
    offset_ += static_cast<std::uint32_t>(content.size());
}

bool DirectWebVolumeUpdateService::finish(DirectWebVolumeUpdatePort& port) const {
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
