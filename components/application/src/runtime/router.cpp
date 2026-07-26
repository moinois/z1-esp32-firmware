// Implements routing precedence without performing queue or transport operations.
#include "firmware/application/router.hpp"

#include "firmware/core/protocol_constants.hpp"
#include "firmware/core/text.hpp"

#include <algorithm>
#include <string_view>

namespace firmware::application {
namespace {

bool starts_with(core::BytesView payload, std::string_view prefix) {
    return payload.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), payload.begin());
}

bool is_file_data_type(std::uint8_t type) {
    return type >= core::protocol::file_md5 && type <= core::protocol::file_retry;
}

ControllerFamily controller_family(std::uint8_t type) {
    switch (type & core::protocol::family_mask) {
        case core::protocol::firmware_family:
            return ControllerFamily::firmware;
        case core::protocol::configuration_family:
            return ControllerFamily::configuration;
        case core::protocol::factory_family:
            return ControllerFamily::factory_data;
        case core::protocol::play_family:
            return ControllerFamily::streamed_play;
        default:
            return ControllerFamily::none;
    }
}

}  // namespace

bool RouteDecision::has(RouteTarget target) const {
    return (targets & static_cast<std::uint16_t>(target)) != 0U;
}

void RouteDecision::add(RouteTarget target) {
    targets |= static_cast<std::uint16_t>(target);
}

void RouteDecision::suppress_controller() {
    targets &= static_cast<std::uint16_t>(~static_cast<std::uint16_t>(RouteTarget::controller));
    controller_requires_local_acceptance = false;
    if (targets == 0U) {
        add(RouteTarget::consume);
    }
}

RouteDecision Router::from_controller(const core::Frame& frame) const {
    RouteDecision decision;
    decision.controller_family = controller_family(frame.type);
    if (decision.controller_family != ControllerFamily::none) {
        decision.add(RouteTarget::consume);
        return decision;
    }

    switch (frame.type) {
        case core::protocol::machine_status:
            decision.add(RouteTarget::status_snapshot);
            break;
        case core::protocol::diagnostic_data:
            decision.add(RouteTarget::diagnostic_snapshot);
            break;
        case core::protocol::controller_version:
            decision.add(RouteTarget::version_snapshot);
            break;
        default:
            decision.add(RouteTarget::broadcast);
            break;
    }
    return decision;
}

RouteDecision Router::from_host(const HostIdentity& host, const core::Frame& frame) const {
    RouteDecision decision;
    const core::BytesView payload(frame.payload);

    if (frame.type == core::protocol::file_command &&
        (starts_with(payload, "upload") || starts_with(payload, "download"))) {
        decision.add(RouteTarget::file_transfer);
        return decision;
    }
    if (is_file_data_type(frame.type)) {
        decision.add(ownership_.is_file_owner(host) ? RouteTarget::file_transfer : RouteTarget::consume);
        return decision;
    }
    if (frame.type == core::protocol::play_status) {
        decision.add(RouteTarget::play_status);
        return decision;
    }
    if (frame.type == core::protocol::single_command) {
        decision.add(!frame.payload.empty() && frame.payload.front() == '?' ? RouteTarget::local_command
                                                                           : RouteTarget::controller);
    } else if (frame.type == core::protocol::general_command && starts_with(payload, "play")) {
        decision.add(RouteTarget::local_command);
        decision.add(RouteTarget::controller);
        decision.controller_requires_local_acceptance = true;
    } else if (frame.type == core::protocol::general_command && starts_with(payload, "M942")) {
        decision.add(RouteTarget::local_command);
        decision.add(RouteTarget::controller);
    } else if (frame.type == core::protocol::general_command) {
        const auto command = core::recognize_command(payload);
        decision.add(command.kind != core::CommandKind::unknown && command.accepted ? RouteTarget::local_command
                                                                                    : RouteTarget::controller);
    } else {
        decision.add(RouteTarget::controller);
    }

    if (controller_transfer_active_ && decision.has(RouteTarget::controller)) {
        decision.suppress_controller();
    }
    return decision;
}

void Router::apply_controller_admission(RouteDecision& decision, std::size_t encoded_size,
                                        bool output_capacity_available) {
    if (!decision.has(RouteTarget::controller)) {
        return;
    }
    if (encoded_size == 0U || encoded_size > core::protocol::controller_maximum_item_size ||
        !output_capacity_available) {
        decision.suppress_controller();
    }
}

}  // namespace firmware::application
