// Implements routing precedence without performing queue or transport operations.
#include "firmware/application/router.hpp"

#include "firmware/core/text.hpp"

#include <algorithm>
#include <string_view>

namespace firmware::application {
namespace {

bool starts_with(core::BytesView payload, std::string_view prefix) {
    return payload.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), payload.begin());
}

bool is_file_data_type(std::uint8_t type) {
    return type >= 0xB1U && type <= 0xB6U;
}

ControllerFamily controller_family(std::uint8_t type) {
    switch (type & 0xF0U) {
        case 0xC0U:
            return ControllerFamily::firmware;
        case 0xD0U:
            return ControllerFamily::configuration;
        case 0xE0U:
            return ControllerFamily::factory_data;
        case 0xF0U:
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
        case 0x81U:
            decision.add(RouteTarget::status_snapshot);
            break;
        case 0x82U:
            decision.add(RouteTarget::diagnostic_snapshot);
            break;
        case 0x71U:
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

    if (frame.type == 0xB0U && (starts_with(payload, "upload") || starts_with(payload, "download"))) {
        decision.add(RouteTarget::file_transfer);
        return decision;
    }
    if (is_file_data_type(frame.type)) {
        decision.add(ownership_.is_file_owner(host) ? RouteTarget::file_transfer : RouteTarget::consume);
        return decision;
    }
    if (frame.type == 0xB7U) {
        decision.add(RouteTarget::play_status);
        return decision;
    }
    if (frame.type == 0xA1U) {
        decision.add(!frame.payload.empty() && frame.payload.front() == '?' ? RouteTarget::local_command
                                                                           : RouteTarget::controller);
    } else if (frame.type == 0xA2U && starts_with(payload, "play")) {
        decision.add(RouteTarget::local_command);
        decision.add(RouteTarget::controller);
        decision.controller_requires_local_acceptance = true;
    } else if (frame.type == 0xA2U && starts_with(payload, "M942")) {
        decision.add(RouteTarget::local_command);
        decision.add(RouteTarget::controller);
    } else if (frame.type == 0xA2U) {
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
    if (encoded_size == 0U || encoded_size > 544U || !output_capacity_available) {
        decision.suppress_controller();
    }
}

}  // namespace firmware::application
