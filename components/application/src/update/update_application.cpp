/** @file @brief Implements controller staging and mainboard OTA application state ordering. */
#include "application/update/update_application.hpp"
#include "application/update/update_phase.hpp"
#include "core/filesystem/sd_user_path.hpp"
#include "core/update/update_package.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace firmware::application {
namespace {

const std::string aggregate_path = core::physical_sd_path("/firmware.bin");
const std::string controller_image_path =
    core::physical_sd_path("/lpc1768.bin");

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

    port_.publish_phase(update_mainboard_phase);
    if (!port_.select_inactive_partition()) {
        return fail_mainboard(false);
    }
    if (!port_.begin_mainboard_write(package.header.mainboard_size)) {
        return fail_mainboard(false);
    }

    const core::BytesView mainboard_image(
        package.bytes.data() + core::update_package_header_size,
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
    port_.persist_phase_direct(update_controller_phase);
    port_.remove_aggregate(aggregate_path);
    port_.restart_mainboard();
    return true;
}

void UpdateApplicationService::stage_controller(
    const ValidatedUpdatePackage& package) {
    if (package.header.controller_size == 0U) {
        return;
    }
    const std::uint32_t controller_offset = static_cast<std::uint32_t>(
        core::update_package_header_size + package.header.mainboard_size);
    const core::BytesView controller_image(
        package.bytes.data() + controller_offset,
        package.header.controller_size);
    port_.stage_controller(controller_image_path, controller_image);
}

bool UpdateApplicationService::fail_mainboard(bool write_active) {
    if (write_active) {
        port_.abort_mainboard_write();
    }
    port_.publish_phase(update_failure_phase);
    return false;
}

}  // namespace firmware::application
