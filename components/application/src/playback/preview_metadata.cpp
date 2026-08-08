/** @file @brief Implements compact, ordered preview open/meta JSON serialization. */
#include "firmware/application/preview_metadata.hpp"

#include <string>

namespace firmware::application {
namespace {

void append_string(std::string& out, std::string_view value) {
    out.push_back('"');
    for (const char c : value) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    out.push_back('"');
}

std::string_view filename(std::string_view path) {
    const auto slash = path.find_last_of('/');
    return slash == std::string_view::npos ? path : path.substr(slash + 1U);
}

}  // namespace

std::string format_preview_metadata(const core::AviPreview& avi,
                                    std::string_view session_id,
                                    std::string_view path,
                                    std::uint32_t sequence,
                                    std::uint64_t first_frame_index) {
    const std::uint64_t period = avi.frame_period_us;
    const std::uint64_t fps = period == 0U ? 10U : 1000000U / period;
    const std::uint64_t duration_ms =
        (static_cast<std::uint64_t>(avi.entries.size()) * period) / 1000U;
    std::string out = "{\"ns\":\"vpreview\",\"rsp\":\"meta\",\"seq\":" +
                      std::to_string(sequence) + ",\"err\":0,\"session_id\":";
    append_string(out, session_id);
    out += ",\"path\":";
    append_string(out, path);
    out += ",\"filename\":";
    append_string(out, filename(path));
    out += ",\"total_frames\":" + std::to_string(avi.entries.size()) +
           ",\"fps\":" + std::to_string(fps) +
           ",\"frame_period_us\":" + std::to_string(period) +
           ",\"duration_ms\":" + std::to_string(duration_ms) +
           ",\"width\":" + std::to_string(avi.width) +
           ",\"height\":" + std::to_string(avi.height) +
           ",\"first_frame_index\":" + std::to_string(first_frame_index) +
           ",\"stream\":\"jpeg\"}";
    return out;
}

}  // namespace firmware::application
