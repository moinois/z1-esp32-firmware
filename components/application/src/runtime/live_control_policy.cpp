/** @file @brief Implements live-video start/stop ownership and cross-socket preemption. */
#include "application/runtime/live_control_policy.hpp"

namespace firmware::application {

std::vector<LiveControlDecision> LiveControlPolicy::handle(
    std::uint32_t socket_id, std::string_view payload) {
    if (payload == "stop_stream") {
        if (owner_.has_value() && *owner_ == socket_id) {
            owner_.reset();
            return {{LiveControlAction::stop, socket_id}};
        }
        return {};
    }
    if (payload != "start_stream") {
        return {};
    }
    if (owner_.has_value() && *owner_ == socket_id) {
        return {};
    }
    std::vector<LiveControlDecision> decisions;
    if (owner_.has_value()) {
        decisions.push_back({LiveControlAction::stop, *owner_});
        decisions.push_back({LiveControlAction::preempted, *owner_});
    }
    owner_ = socket_id;
    decisions.push_back({LiveControlAction::start, socket_id});
    return decisions;
}

std::vector<LiveControlDecision> LiveControlPolicy::on_disconnect(
    std::uint32_t socket_id) {
    if (!owner_.has_value() || *owner_ != socket_id) {
        return {};
    }
    owner_.reset();
    return {{LiveControlAction::stop, socket_id}};
}

}  // namespace firmware::application
