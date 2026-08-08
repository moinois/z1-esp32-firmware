/** @file @brief Implements the atomic process-wide recording request flag. */
#include "recording_request_state.hpp"

#include <atomic>

namespace firmware::target {
namespace {

std::atomic_bool recording_requested{false};

}  // namespace

void RecordingRequestState::set_requested(bool requested) {
    recording_requested.store(requested, std::memory_order_release);
}

bool RecordingRequestState::requested() const {
    return recording_requested.load(std::memory_order_acquire);
}

}  // namespace firmware::target
