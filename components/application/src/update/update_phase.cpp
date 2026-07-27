// Implements update recovery, bounded phase publication, and visible progress.
#include "firmware/application/update_phase.hpp"
#include "firmware/core/protocol_constants.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace firmware::application {
namespace {

constexpr std::uint8_t maximum_progress = 100U;
constexpr std::uint64_t error_broadcast_interval_milliseconds = 1000U;
constexpr std::uint64_t success_display_milliseconds = 3000U;
constexpr std::string_view validation_error =
    "Error: The firmware file format is incorrect or it has been damaged. "
    "Please re-upload";
constexpr std::string_view previous_failure_error =
    "Error: Previous firmware upgrade failed. Please re-upload the firmware "
    "package.";

}  // namespace

UpdatePhaseService::UpdatePhaseService(UpdatePhasePort& port) : port_(port) {}

void UpdatePhaseService::reconcile_boot(std::uint8_t persisted_phase,
                                        std::uint64_t now_milliseconds) {
    persisted_phase_ = persisted_phase;
    if (persisted_phase == update_failure_phase) {
        broadcast_limited(previous_failure_error, now_milliseconds);
    } else if (persisted_phase == update_success_phase &&
               port_.persist_phase(update_idle_phase)) {
        persisted_phase_ = update_idle_phase;
    }
}

void UpdatePhaseService::aggregate_opened() {
    if (persisted_phase_ == update_failure_phase &&
        port_.persist_phase(update_idle_phase)) {
        persisted_phase_ = update_idle_phase;
    }
}

void UpdatePhaseService::publish(std::uint8_t phase) {
    volatile_status_ = {phase, 0U};
    success_deadline_milliseconds_.reset();
    if (pending_count_ < pending_phases_.size()) {
        pending_phases_[pending_count_] = phase;
        ++pending_count_;
    }
}

void UpdatePhaseService::process_pending() {
    if (pending_count_ == 0U) {
        return;
    }
    const std::uint8_t newest_phase = pending_phases_[pending_count_ - 1U];
    pending_count_ = 0U;
    if (port_.persist_phase(newest_phase)) {
        persisted_phase_ = newest_phase;
    }
}

std::size_t UpdatePhaseService::pending_count() const {
    return pending_count_;
}

void UpdatePhaseService::broadcast_validation_error(
    std::uint64_t now_milliseconds) {
    broadcast_limited(validation_error, now_milliseconds);
}

void UpdatePhaseService::set_controller_progress(std::uint32_t index,
                                                 std::uint32_t frame_count) {
    if (volatile_status_.phase != update_controller_phase || frame_count == 0U) {
        return;
    }
    const std::uint64_t rounded =
        (100ULL * index + frame_count / 2U) / frame_count;
    volatile_status_.progress = static_cast<std::uint8_t>(
        std::min<std::uint64_t>(rounded, maximum_progress));
}

void UpdatePhaseService::controller_completed(
    std::uint64_t now_milliseconds) {
    publish(update_success_phase);
    volatile_status_.progress = maximum_progress;
    success_deadline_milliseconds_ =
        now_milliseconds + success_display_milliseconds;
}

void UpdatePhaseService::tick(std::uint64_t now_milliseconds) {
    if (success_deadline_milliseconds_.has_value() &&
        now_milliseconds >= *success_deadline_milliseconds_) {
        volatile_status_ = {update_idle_phase, 0U};
        success_deadline_milliseconds_.reset();
    }
}

UpdateStatus UpdatePhaseService::status() const {
    if (volatile_status_.phase != update_idle_phase) {
        return volatile_status_;
    }
    if (persisted_phase_ == update_failure_phase) {
        return {update_failure_phase, 0U};
    }
    return {update_idle_phase, 0U};
}

void UpdatePhaseService::broadcast_limited(
    std::string_view message, std::uint64_t now_milliseconds) {
    if (last_error_broadcast_milliseconds_.has_value()) {
        const std::uint64_t last = *last_error_broadcast_milliseconds_;
        if (now_milliseconds >= last &&
            now_milliseconds - last < error_broadcast_interval_milliseconds) {
            return;
        }
    }
    port_.broadcast(core::protocol::console_message, message);
    last_error_broadcast_milliseconds_ = now_milliseconds;
}

}  // namespace firmware::application
