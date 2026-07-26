// Implements deterministic preview command ownership and frame selection rules.
#include "firmware/application/preview_playback.hpp"

#include <algorithm>

namespace firmware::application {
namespace {

std::uint32_t clamp_frame(std::uint64_t frame, std::uint32_t last) {
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(frame, last));
}

}  // namespace

PreviewPlaybackResult apply_preview_command(const PreviewRequest& request,
                                            PreviewMode& mode,
                                            std::uint32_t& current_frame,
                                            std::uint32_t last_frame,
                                            std::uint32_t frame_period_us,
                                            bool session_matches,
                                            bool session_active) {
    PreviewPlaybackResult result;
    if (!session_active) {
        if (request.command != PreviewCommand::open) {
            result.reply = true;
            result.response = "conflict";
        }
        return result;
    }
    if (!session_matches && mode == PreviewMode::playing) return result;
    if (!session_matches && mode == PreviewMode::paused &&
        request.command != PreviewCommand::play && request.command != PreviewCommand::resume) {
        if (request.command != PreviewCommand::stop) return result;
        mode = PreviewMode::stopped;
        result.terminated = true;
        return result;
    }
    switch (request.command) {
        case PreviewCommand::play:
            if (request.from_frame >= 0) {
                current_frame = clamp_frame(static_cast<std::uint64_t>(request.from_frame), last_frame);
            } else if (request.from_milliseconds != 0U && frame_period_us != 0U) {
                current_frame = clamp_frame(request.from_milliseconds * 1000U / frame_period_us, last_frame);
            }
            mode = PreviewMode::playing;
            result.reply = true;
            break;
        case PreviewCommand::pause:
            if (mode == PreviewMode::paused) return result;
            mode = PreviewMode::paused;
            result.reply = true;
            break;
        case PreviewCommand::resume:
            mode = PreviewMode::playing;
            result.reply = true;
            break;
        case PreviewCommand::seek: {
            std::uint64_t selected = request.frame >= 0
                ? static_cast<std::uint64_t>(request.frame)
                : (frame_period_us == 0U ? 0U : request.time_milliseconds * 1000U / frame_period_us);
            current_frame = clamp_frame(selected, last_frame);
            result.frame = current_frame;
            result.time_milliseconds = frame_period_us == 0U ? 0U : (static_cast<std::uint64_t>(current_frame) * frame_period_us) / 1000U;
            result.reply = true;
            break;
        }
        case PreviewCommand::stop: {
            const bool was_paused = mode == PreviewMode::paused;
            mode = PreviewMode::stopped;
            result.terminated = true;
            result.reply = !was_paused;
            break;
        }
        case PreviewCommand::open:
            break;
    }
    return result;
}

}  // namespace firmware::application
