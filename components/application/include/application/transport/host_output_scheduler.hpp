/** @file @brief Globally bounds and orders output for every host transport. */
#pragma once

#include "application/runtime/ownership.hpp"
#include "core/protocol/frame.hpp"

#include <cstddef>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <vector>

namespace firmware::application {

/** Identifies whether an admitted frame retains one host or is broadcast. */
struct HostOutputDestination {
    /// Retained destination; no value denotes a broadcast.
    std::optional<HostIdentity> host;

    /// Creates an output destination retained for one logical host slot.
    static HostOutputDestination addressed(HostIdentity identity);
    /// Creates a destination expanded in USB-then-TCP order when selected.
    static HostOutputDestination broadcast();
};

/** Distinguishes arrivals whose overflow semantics purge the shared queue. */
enum class HostOutputSource {
    ordinary,
    motion_board_unchanged,
    inactivity_alarm,
    rate_limited_console_error
};

/** Reports why a frame was retained or discarded at global admission. */
enum class HostOutputAdmission {
    accepted,
    capacity_drop,
    purged_at_capacity,
    allocation_failure
};

/** One retained output item with its immutable delivery destination. */
struct PendingHostOutput {
    core::Frame frame;
    HostOutputDestination destination;
};

/** One TRN-006 selection group, delivered before another selection occurs. */
struct HostOutputSelection {
    std::optional<PendingHostOutput> download_data;
    std::vector<PendingHostOutput> non_download;
    bool delay_before_next_selection = true;
};

/** Owns the two firmware-wide pending-output capacities from TRN-005/006.
 *
 * Admission and selection are serialized so USB and every TCP producer observe
 * one pair of limits. Transport adapters remain responsible for delivery and
 * for applying the nominal interval requested by each selection.
 */
class HostOutputScheduler {
public:
    static constexpr std::size_t maximum_download_data = 32U;
    static constexpr std::size_t maximum_non_download = 32U;

    /** Retains a complete decoded frame under the applicable global limit.
     * @param frame Application frame; B3 payloads over four bytes use the
     * download-data capacity.
     * @param destination Retained addressed host or deferred broadcast.
     * @param source Arrival class controlling full-queue purge behavior.
     */
    HostOutputAdmission admit(
        const core::Frame& frame, HostOutputDestination destination,
        HostOutputSource source = HostOutputSource::ordinary) noexcept;

    /** Gives an `ls` response its sole permitted bounded capacity wait.
     * @param maximum_wait Nominal target wait is 300 milliseconds; an explicit
     * duration keeps portable tests deterministic.
     */
    HostOutputAdmission admit_listing(
        const core::Frame& frame, HostOutputDestination destination,
        std::chrono::milliseconds maximum_wait) noexcept;

    /** Updates whether any eligible destination exists.
     * Clearing the last destination immediately drops pending non-download
     * output while deliberately retaining download data.
     */
    void set_active_destinations(bool usb_active, bool tcp_active);

    /// Removes and returns the next normative one-data-plus-other selection.
    HostOutputSelection select();

    /// Reports retained download-data responses for diagnostics and tests.
    std::size_t pending_download_data() const;
    /// Reports all other retained host-output frames.
    std::size_t pending_non_download() const;

private:
    static bool is_download_data(const core::Frame& frame);
    static bool purges_on_full(HostOutputSource source);

    mutable std::mutex mutex_;
    std::condition_variable capacity_changed_;
    std::deque<PendingHostOutput> download_data_;
    std::deque<PendingHostOutput> non_download_;
    bool usb_active_ = false;
    bool tcp_active_ = false;
};

}  // namespace firmware::application
