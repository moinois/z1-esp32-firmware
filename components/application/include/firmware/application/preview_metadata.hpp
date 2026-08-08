/** @file @brief Declares the deterministic JSON metadata response for an opened preview. */
#pragma once

#include "firmware/core/avi_preview.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace firmware::application {

/// Formats the PREV-013/PREV-014 fields from an accepted AVI and session.
std::string format_preview_metadata(const core::AviPreview& avi,
                                    std::string_view session_id,
                                    std::string_view path,
                                    std::uint32_t sequence,
                                    std::uint64_t first_frame_index = 0U);

}  // namespace firmware::application
