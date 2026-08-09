/** @file @brief Implements preview path, file, AVI, buffer, and session admission rules. */
#include "application/playback/preview_open.hpp"

#include "core/media/preview_path_policy.hpp"

#include <iomanip>
#include <sstream>

namespace firmware::application {
namespace {

constexpr std::size_t maximum_preview_frame_size = 256U * 1024U;
constexpr std::size_t small_frame_threshold = 512U;
constexpr std::size_t small_frame_buffer_size = 64U * 1024U;

}  // namespace

PreviewOpenDecision decide_preview_open(std::string_view path,
                                        const core::AviPreview* avi,
                                        bool file_exists,
                                        bool file_opened,
                                        std::uint32_t random_high,
                                        std::uint32_t random_low) {
    PreviewOpenDecision decision;
    if (!core::preview_path_allowed(path)) {
        decision.error = PreviewOpenError::path_not_allowed;
        return decision;
    }
    if (!file_exists) {
        decision.error = PreviewOpenError::missing_file;
        return decision;
    }
    if (!file_opened) {
        decision.error = PreviewOpenError::open_failed;
        return decision;
    }
    if (avi == nullptr) {
        decision.error = PreviewOpenError::invalid_avi;
        return decision;
    }
    std::size_t largest = 0U;
    for (const auto& entry : avi->entries) largest = std::max(largest, static_cast<std::size_t>(entry.advertised_size));
    if (largest > maximum_preview_frame_size) {
        decision.error = PreviewOpenError::buffer_too_large;
        return decision;
    }
    decision.frame_buffer_size =
        largest < small_frame_threshold ? small_frame_buffer_size : largest;
    std::ostringstream id;
    id << "pv-" << std::hex << std::setfill('0') << std::setw(8) << random_high
       << '-' << std::setw(8) << random_low;
    decision.session_id = id.str();
    return decision;
}

}  // namespace firmware::application
