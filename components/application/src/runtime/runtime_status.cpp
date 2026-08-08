/** @file @brief Composes independent runtime observations into host-visible status fields. */
#include "firmware/application/runtime_status.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace firmware::application {
namespace {

// Narrows a capacity without wrapping if a future volume exceeds the wire field.
std::uint32_t bounded_mib(std::uint64_t value) {
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(
        value, std::numeric_limits<std::uint32_t>::max()));
}

}  // namespace

AggregatedStatusService::AggregatedStatusService(
    const AggregatedStatusPort& port)
    : port_(port) {}

core::StatusExtension AggregatedStatusService::extension() const {
    core::StatusExtension result;
    result.transfer_active = port_.host_transfer_active();
    result.record_requested = port_.recording_requested();
    result.recording = port_.recording_active();

    const std::optional<SdCapacity> capacity = port_.sd_capacity();
    if (capacity.has_value()) {
        const std::uint64_t used =
            capacity->total_mib > capacity->free_mib
                ? capacity->total_mib - capacity->free_mib
                : 0U;
        result.sd_used_mib = bounded_mib(used);
        result.sd_total_mib = bounded_mib(capacity->total_mib);
    }

    const UpdateStatus update = port_.update_status();
    result.update_phase = update.phase;
    result.update_progress = update.progress;
    return result;
}

std::int32_t AggregatedStatusService::diagnostic_rssi() const {
    return port_.station_rssi().value_or(0);
}

}  // namespace firmware::application
