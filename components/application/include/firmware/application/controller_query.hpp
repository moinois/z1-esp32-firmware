/** @file @brief Defines the transport-independent schedule for periodic controller queries. */
#pragma once

#include "firmware/core/frame.hpp"

#include <cstdint>
#include <vector>

namespace firmware::application {

/** Schedules periodic controller status and diagnostic query packets. */
class ControllerQueryScheduler {
public:
    /// Starts both schedules at the supplied monotonic millisecond value.
    explicit ControllerQueryScheduler(std::uint64_t start_milliseconds);

    /// Returns queries due now and advances all elapsed schedule opportunities.
    std::vector<core::Frame> poll(std::uint64_t now_milliseconds, bool controller_traffic_allowed);

private:
    std::uint64_t next_status_milliseconds_;
    std::uint64_t next_diagnostic_milliseconds_;
};

}  // namespace firmware::application
