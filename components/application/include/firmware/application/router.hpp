// Defines side-effect-free routing decisions for controller and host frames.
#pragma once

#include "firmware/application/ownership.hpp"
#include "firmware/core/frame.hpp"

#include <cstddef>
#include <cstdint>

namespace firmware::application {

enum class RouteTarget : std::uint16_t {
    none = 0,
    consume = 1U << 0U,
    broadcast = 1U << 1U,
    controller = 1U << 2U,
    local_command = 1U << 3U,
    file_transfer = 1U << 4U,
    play_status = 1U << 5U,
    status_snapshot = 1U << 6U,
    diagnostic_snapshot = 1U << 7U,
    version_snapshot = 1U << 8U
};

enum class ControllerFamily {
    none,
    firmware,
    configuration,
    factory_data,
    streamed_play
};

struct RouteDecision {
    std::uint16_t targets = 0;
    ControllerFamily controller_family = ControllerFamily::none;
    bool controller_requires_local_acceptance = false;

    // Reports whether the decision includes a particular destination.
    bool has(RouteTarget target) const;

    // Adds one destination without disturbing previously selected destinations.
    void add(RouteTarget target);

    // Removes controller forwarding when a transfer suppresses ordinary traffic.
    void suppress_controller();
};

class Router {
public:
    // Selects the consumer for one structurally valid controller frame.
    RouteDecision from_controller(const core::Frame& frame) const;

    // Selects consumers for one structurally valid frame from a host connection.
    RouteDecision from_host(const HostIdentity& host, const core::Frame& frame) const;

    // Applies the size and output-capacity gates to selected controller output.
    static void apply_controller_admission(RouteDecision& decision, std::size_t encoded_size,
                                           bool output_capacity_available);

    // Exposes the ownership state used during host routing admission.
    Ownership& ownership() {
        return ownership_;
    }

    // Exposes ownership as read-only state for status aggregation.
    const Ownership& ownership() const {
        return ownership_;
    }

    // Enables suppression during firmware, configuration, or factory transfers.
    void set_controller_transfer_active(bool active) {
        controller_transfer_active_ = active;
    }

private:
    Ownership ownership_;
    bool controller_transfer_active_ = false;
};

}  // namespace firmware::application
