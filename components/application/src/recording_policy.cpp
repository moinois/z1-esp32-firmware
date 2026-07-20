// Implements recording path validation, UTC naming, and interval accounting.
#include "firmware/application/recording_policy.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace firmware::application {

std::optional<std::string> recording_segment_path(
    std::string_view retained_play_path, std::int64_t utc_seconds) {
    const auto slash = retained_play_path.find_last_of('/');
    if (slash == std::string_view::npos || slash + 1U >= retained_play_path.size()) {
        return std::nullopt;
    }
    const std::string_view filename = retained_play_path.substr(slash + 1U);
    const auto dot = filename.find('.');
    if (dot == std::string_view::npos || dot == 0U) return std::nullopt;
    std::tm utc{};
    const std::time_t seconds = static_cast<std::time_t>(utc_seconds);
    if (gmtime_r(&seconds, &utc) == nullptr) return std::nullopt;
    std::ostringstream output;
    output << "/sd/videos/" << filename.substr(0U, dot) << '-'
           << std::setfill('0') << std::setw(4) << utc.tm_year + 1900
           << std::setw(2) << utc.tm_mon + 1 << std::setw(2) << utc.tm_mday
           << '_' << std::setw(2) << utc.tm_hour << std::setw(2) << utc.tm_min
           << std::setw(2) << utc.tm_sec << ".avi";
    const std::string path = output.str();
    if (path.size() > 255U) return std::nullopt;
    return path;
}

bool recording_conditions_active(bool recording_requested,
                                 bool streamed_play_running,
                                 bool controller_running) {
    return recording_requested && (streamed_play_running || controller_running);
}

bool advance_recording_segment(RecordingSegmentState& state,
                               bool conditions_active,
                               bool capture_succeeded,
                               bool write_succeeded,
                               std::size_t frames_of_one_file) {
    ++state.attempted_frames;
    if (capture_succeeded && write_succeeded) ++state.successful_frames;
    if (!conditions_active) ++state.inactive_intervals;
    if (frames_of_one_file == 0U) return true;
    if (state.attempted_frames >= frames_of_one_file) return true;
    return state.inactive_intervals > 30U;
}

}  // namespace firmware::application
