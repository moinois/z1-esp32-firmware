/** @file @brief Declares one deterministic preview frame scheduling step. */
#pragma once

#include <cstddef>
#include <cstdint>

namespace firmware::application {

/** Action selected for one preview worker iteration. */
enum class PreviewFrameAction { send_frame, delay, eos, terminate };

/** Worker action paired with any monotonic delay required before it. */
struct PreviewFrameStep {
    PreviewFrameAction action = PreviewFrameAction::terminate;
    std::size_t frame_index = 0U;
    std::uint32_t delay_milliseconds = 0U;
};

/// Selects the next frame action and applies the specified playback timing policy.
PreviewFrameStep schedule_preview_frame(std::size_t current_frame,
                                         std::size_t frame_count,
                                         std::uint32_t frame_period_us,
                                         bool read_succeeded,
                                         bool send_succeeded,
                                         bool connection_alive,
                                         bool explicitly_stopped);

}  // namespace firmware::application
