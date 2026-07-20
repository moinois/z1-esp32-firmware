// Declares portable recording naming and segment lifecycle rules.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace firmware::application {

// Builds one UTC recording segment path from a retained streamed-play path.
std::optional<std::string> recording_segment_path(
    std::string_view retained_play_path, std::int64_t utc_seconds);

// Reports whether recording conditions are eligible for a capture interval.
bool recording_conditions_active(bool recording_requested,
                                 bool streamed_play_running,
                                 bool controller_running);

struct RecordingSegmentState {
    std::size_t attempted_frames = 0U;
    std::size_t successful_frames = 0U;
    std::size_t inactive_intervals = 0U;
    bool initialized = false;
};

// Advances one capture interval and reports whether the segment must close.
bool advance_recording_segment(RecordingSegmentState& state,
                               bool conditions_active,
                               bool capture_succeeded,
                               bool write_succeeded,
                               std::size_t frames_of_one_file);

}  // namespace firmware::application
