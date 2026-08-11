/** @file @brief Composes the firmware-wide host-output scheduler with USB/TCP. */
#pragma once

#include "application/runtime/ownership.hpp"
#include "application/transport/host_output_scheduler.hpp"
#include "core/protocol/frame.hpp"

namespace firmware::target {

/// Starts the single output-selection task; repeated calls are harmless.
bool initialize_host_output_adapter();

/// Admits one addressed frame under the global TRN-005/TRN-006 limits.
bool queue_host_frame(
    const firmware::core::Frame& frame,
    firmware::application::HostIdentity destination,
    firmware::application::HostOutputSource source =
        firmware::application::HostOutputSource::ordinary);

/// Admits an `ls` response with its sole permitted nominal 300 ms wait.
bool queue_host_listing(const firmware::core::Frame& frame,
                        firmware::application::HostIdentity destination);

/// Admits one broadcast expanded in USB-then-TCP order during delivery.
bool broadcast_host_frame(
    const firmware::core::Frame& frame,
    firmware::application::HostOutputSource source =
        firmware::application::HostOutputSource::ordinary);

/** Admits a broadcast while preserving the detailed overflow result.
 * This form lets the play adapter distinguish its normative catastrophic
 * queue purge from an ordinary no-destination or allocation rejection.
 */
firmware::application::HostOutputAdmission admit_host_broadcast(
    const firmware::core::Frame& frame,
    firmware::application::HostOutputSource source =
        firmware::application::HostOutputSource::ordinary);

/// Updates USB destination availability and applies the no-host purge rule.
void set_host_output_usb_active(bool active);

/// Updates TCP destination availability and applies the no-host purge rule.
void set_host_output_tcp_active(bool active);

}  // namespace firmware::target
