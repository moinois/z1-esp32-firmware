/** @file @brief Implements bounded frame delivery, tick-rounded delay, and clean EOS handling. */
#include "firmware/application/preview_frame_step.hpp"

namespace firmware::application {

PreviewFrameStep schedule_preview_frame(std::size_t current_frame,
                                         std::size_t frame_count,
                                         std::uint32_t frame_period_us,
                                         bool read_succeeded,
                                         bool send_succeeded,
                                         bool connection_alive,
                                         bool explicitly_stopped) {
    PreviewFrameStep step;
    step.frame_index = current_frame;
    if (!connection_alive || explicitly_stopped || !read_succeeded || !send_succeeded) {
        step.action = PreviewFrameAction::terminate;
        return step;
    }
    if (current_frame >= frame_count) {
        step.action = PreviewFrameAction::eos;
        return step;
    }
    step.action = PreviewFrameAction::send_frame;
    const std::uint32_t rounded_ticks = frame_period_us / 10000U;
    step.delay_milliseconds = rounded_ticks == 0U ? 10U : rounded_ticks * 10U;
    return step;
}

}  // namespace firmware::application
