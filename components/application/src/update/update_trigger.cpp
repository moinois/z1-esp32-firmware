/** @file @brief Implements exact update command recognition and one-request coalescing. */
#include "firmware/application/update_trigger.hpp"
#include "firmware/core/sd_user_path.hpp"

#include <string_view>

namespace firmware::application {
namespace {

const std::string partial_aggregate_path =
    core::physical_sd_path("/firmware.bin.part");
constexpr std::string_view upgrade_prefix = "upgrade";
constexpr std::string_view reset_prefix = "reset";

// Reports whether text begins with the exact case-sensitive prefix.
bool begins_with(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() &&
           text.substr(0U, prefix.size()) == prefix;
}

}  // namespace

UpdateTriggerService::UpdateTriggerService(UpdateTriggerPort& port)
    : port_(port) {}

void UpdateTriggerService::boot() {
    port_.remove_partial(partial_aggregate_path);
    port_.reconcile_persisted_state();
    request_processing();
}

bool UpdateTriggerService::handle_command(std::string_view command) {
    if (!begins_with(command, upgrade_prefix) &&
        !begins_with(command, reset_prefix)) {
        return false;
    }
    request_processing();
    return true;
}

bool UpdateTriggerService::take_request() {
    const bool was_pending = request_pending_;
    request_pending_ = false;
    return was_pending;
}

void UpdateTriggerService::request_processing() {
    request_pending_ = true;
}

}  // namespace firmware::application
