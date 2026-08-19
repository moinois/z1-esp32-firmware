/** @file @brief Implements controller update reset scheduling and transfer outcome effects. */
#include "application/update/update_controller.hpp"
#include "core/filesystem/sd_user_path.hpp"

#include <cstdint>
#include <string_view>

namespace firmware::application {
namespace {

constexpr std::uint64_t controller_check_interval_milliseconds = 5000U;
const std::string staged_controller_path =
    core::physical_sd_path("/lpc1768.bin");

}  // namespace

UpdateControllerMonitor::UpdateControllerMonitor(UpdateControllerPort& port)
    : port_(port) {}

void UpdateControllerMonitor::start(std::uint64_t now_milliseconds) {
    next_check_milliseconds_ = now_milliseconds;
    started_ = true;
}

void UpdateControllerMonitor::tick(std::uint64_t now_milliseconds) {
    if (!started_ || now_milliseconds < next_check_milliseconds_) {
        return;
    }
    do {
        next_check_milliseconds_ += controller_check_interval_milliseconds;
    } while (next_check_milliseconds_ <= now_milliseconds);

    if (port_.staged_controller_exists() && transfer_channels_idle()) {
        port_.send_controller_reset();
    }
}

void UpdateControllerMonitor::transfer_failed() {
    publish_failure_if_available();
}

void UpdateControllerMonitor::transfer_cancelled() {
    port_.publish_error();
}

void UpdateControllerMonitor::transfer_timed_out(bool qualifying) {
    if (qualifying) {
        publish_failure_if_available();
    }
}

void UpdateControllerMonitor::controller_completed(
    std::uint64_t now_milliseconds) {
    if (port_.staged_controller_exists()) {
        port_.remove_staged_controller(staged_controller_path);
    }
    port_.controller_completed(now_milliseconds);
}

void UpdateControllerMonitor::publish_failure_if_available() {
    if (port_.staged_controller_exists()) {
        port_.publish_error();
    }
}

bool UpdateControllerMonitor::transfer_channels_idle() const {
    return !port_.firmware_transfer_active() &&
           !port_.configuration_transfer_active() &&
           !port_.factory_transfer_active();
}

}  // namespace firmware::application
