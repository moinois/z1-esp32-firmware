/** @file @brief Ownership-aware routing decisions for controller and host frames. */
#pragma once

#include "firmware/application/ownership.hpp"
#include "firmware/core/frame.hpp"

#include <cstddef>
#include <cstdint>

namespace firmware::application {

/** Bit destinations that may be combined in one routing decision. */
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

/** Controller transfer family selected from the high packet-type nibble. */
enum class ControllerFamily {
    none,
    firmware,
    configuration,
    factory_data,
    streamed_play
};

/** Complete routing outcome before target adapters perform any I/O. */
struct RouteDecision {
    /// Bitset of @ref RouteTarget values.
    std::uint16_t targets = 0;
    /// Transfer family selected for controller traffic, if any.
    ControllerFamily controller_family = ControllerFamily::none;
    /// Requires the local transfer state machine to accept before forwarding.
    bool controller_requires_local_acceptance = false;

    /// Reports whether the decision includes a destination.
    bool has(RouteTarget target) const;

    /// Adds a destination without disturbing previous selections.
    void add(RouteTarget target);

    /// Removes controller forwarding while retaining all other destinations.
    void suppress_controller();
};

/** Applies shared packet routing, ownership, and transfer-suppression policy. */
class Router {
public:
    /// Selects consumers for one structurally valid controller frame.
    RouteDecision from_controller(const core::Frame& frame) const;

    /// Selects consumers for a valid frame from an identified host connection.
    RouteDecision from_host(const HostIdentity& host, const core::Frame& frame) const;

    /// Applies encoded-size and output-capacity gates to controller forwarding.
    static void apply_controller_admission(RouteDecision& decision, std::size_t encoded_size,
                                           bool output_capacity_available);

    /// Exposes ownership for service-level claim and release operations.
    Ownership& ownership() {
        return ownership_;
    }

    /// Exposes ownership as read-only state for status aggregation.
    const Ownership& ownership() const {
        return ownership_;
    }

    /// Suppresses ordinary controller forwarding during exclusive transfers.
    void set_controller_transfer_active(bool active) {
        controller_transfer_active_ = active;
    }

private:
    Ownership ownership_;
    bool controller_transfer_active_ = false;
};

}  // namespace firmware::application
