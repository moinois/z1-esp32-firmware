// Declares state-only preview playback command transitions.
#pragma once

#include "firmware/application/preview_request.hpp"

#include <cstdint>
#include <string>

namespace firmware::application {

enum class PreviewMode { playing, paused, stopped };

struct PreviewPlaybackResult {
    bool reply = false;
    bool terminated = false;
    std::uint32_t frame = 0U;
    std::uint64_t time_milliseconds = 0U;
    std::string response;
};

// Applies one normalized command without performing filesystem or socket I/O.
PreviewPlaybackResult apply_preview_command(const PreviewRequest& request,
                                            PreviewMode& mode,
                                            std::uint32_t& current_frame,
                                            std::uint32_t last_frame,
                                            std::uint32_t frame_period_us,
                                            bool session_matches,
                                            bool session_active);

}  // namespace firmware::application
