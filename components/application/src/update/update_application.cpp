// Implements controller staging and mainboard OTA application state ordering.
#include "firmware/application/update_application.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace firmware::application {
namespace {

constexpr std::string_view aggregate_path = "/sd/firmware.bin";
constexpr std::string_view controller_image_path = "/sd/lpc1768.bin";
constexpr std::size_t aggregate_header_size = 32U;
constexpr std::uint8_t mainboard_phase = 1U;
constexpr std::uint8_t controller_phase = 2U;
constexpr std::uint8_t failure_phase = 3U;

}  // namespace

UpdateApplicationService::UpdateApplicationService(UpdateApplicationPort& port)
    : port_(port) {}

bool UpdateApplicationService::apply(
    const ValidatedUpdatePackage& package) {
    if (package.header.mainboard_size == 0U) {
        stage_controller(package);
        port_.remove_aggregate(aggregate_path);
        port_.send_controller_reset();
        return true;
    }

    port_.publish_phase(mainboard_phase);
    if (!port_.select_inactive_partition()) {
        return fail_mainboard(false);
    }
    if (!port_.begin_mainboard_write(package.header.mainboard_size)) {
        return fail_mainboard(false);
    }

    const core::BytesView mainboard_image(
        package.bytes.data() + aggregate_header_size,
        package.header.mainboard_size);
    if (!port_.write_mainboard(mainboard_image)) {
        return fail_mainboard(true);
    }
    if (!port_.finalize_mainboard_write()) {
        return fail_mainboard(true);
    }
    if (!port_.select_mainboard_for_boot()) {
        return fail_mainboard(false);
    }

    stage_controller(package);
    port_.persist_phase_direct(controller_phase);
    port_.remove_aggregate(aggregate_path);
    port_.restart_mainboard();
    return true;
}

void UpdateApplicationService::stage_controller(
    const ValidatedUpdatePackage& package) {
    if (package.header.controller_size == 0U) {
        return;
    }
    const std::size_t controller_offset =
        aggregate_header_size + package.header.mainboard_size;
    const core::BytesView controller_image(
        package.bytes.data() + controller_offset,
        package.header.controller_size);
    port_.stage_controller(controller_image_path, controller_image);
}

bool UpdateApplicationService::fail_mainboard(bool write_active) {
    if (write_active) {
        port_.abort_mainboard_write();
    }
    port_.publish_phase(failure_phase);
    return false;
}

}  // namespace firmware::application
