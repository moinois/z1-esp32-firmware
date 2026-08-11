/** @file @brief Implements global host-output admission and batch selection. */
#include "application/transport/host_output_scheduler.hpp"

#include "core/protocol/protocol_constants.hpp"

#include <algorithm>
#include <new>
#include <utility>

namespace firmware::application {

HostOutputDestination HostOutputDestination::addressed(HostIdentity identity) {
    return {identity};
}

HostOutputDestination HostOutputDestination::broadcast() { return {std::nullopt}; }

HostOutputAdmission HostOutputScheduler::admit(
    const core::Frame& frame, HostOutputDestination destination,
    HostOutputSource source) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_download_data(frame) && !usb_active_ && !tcp_active_) {
        return HostOutputAdmission::capacity_drop;
    }
    auto& queue = is_download_data(frame) ? download_data_ : non_download_;
    const std::size_t limit = is_download_data(frame)
                                  ? maximum_download_data
                                  : maximum_non_download;
    if (queue.size() >= limit) {
        if (!is_download_data(frame) && purges_on_full(source)) {
            non_download_.clear();
            return HostOutputAdmission::purged_at_capacity;
        }
        return HostOutputAdmission::capacity_drop;
    }
    try {
        queue.push_back({frame, std::move(destination)});
    } catch (const std::bad_alloc&) {
        return HostOutputAdmission::allocation_failure;
    }
    return HostOutputAdmission::accepted;
}

HostOutputAdmission HostOutputScheduler::admit_listing(
    const core::Frame& frame, HostOutputDestination destination,
    std::chrono::milliseconds maximum_wait) noexcept {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!usb_active_ && !tcp_active_) {
        return HostOutputAdmission::capacity_drop;
    }
    if (!capacity_changed_.wait_for(lock, maximum_wait, [this] {
            return non_download_.size() < maximum_non_download;
        })) {
        return HostOutputAdmission::capacity_drop;
    }
    try {
        non_download_.push_back({frame, std::move(destination)});
    } catch (const std::bad_alloc&) {
        return HostOutputAdmission::allocation_failure;
    }
    return HostOutputAdmission::accepted;
}

void HostOutputScheduler::set_active_destinations(bool usb_active,
                                                  bool tcp_active) {
    std::lock_guard<std::mutex> lock(mutex_);
    usb_active_ = usb_active;
    tcp_active_ = tcp_active;
    if (!usb_active_ && !tcp_active_) {
        non_download_.clear();
        capacity_changed_.notify_all();
    }
}

HostOutputSelection HostOutputScheduler::select() {
    std::lock_guard<std::mutex> lock(mutex_);
    HostOutputSelection result;
    if (!download_data_.empty()) {
        result.download_data = std::move(download_data_.front());
        download_data_.pop_front();
    }
    const std::size_t selected =
        std::min(maximum_non_download, non_download_.size());
    result.non_download.reserve(selected);
    for (std::size_t index = 0U; index < selected; ++index) {
        result.non_download.push_back(std::move(non_download_.front()));
        non_download_.pop_front();
    }
    if (selected != 0U) {
        capacity_changed_.notify_all();
    }
    result.delay_before_next_selection =
        result.download_data.has_value() || result.non_download.empty();
    return result;
}

std::size_t HostOutputScheduler::pending_download_data() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return download_data_.size();
}

std::size_t HostOutputScheduler::pending_non_download() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return non_download_.size();
}

bool HostOutputScheduler::is_download_data(const core::Frame& frame) {
    return frame.type == core::protocol::file_data && frame.payload.size() > 4U;
}

bool HostOutputScheduler::purges_on_full(HostOutputSource source) {
    return source == HostOutputSource::motion_board_unchanged ||
           source == HostOutputSource::inactivity_alarm ||
           source == HostOutputSource::rate_limited_console_error;
}

}  // namespace firmware::application
