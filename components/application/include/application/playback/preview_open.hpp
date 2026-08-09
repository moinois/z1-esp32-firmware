/** @file @brief Declares deterministic preview-open admission and startup sizing policy. */
#pragma once

#include "core/media/avi_preview.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace firmware::application {

/** Validation or resource failure encountered while opening a preview file. */
enum class PreviewOpenError { none, path_not_allowed, missing_file, open_failed, invalid_avi, buffer_too_large };

/** Accepted preview metadata or the explicit reason it could not be opened. */
struct PreviewOpenDecision {
    PreviewOpenError error = PreviewOpenError::none;
    std::string session_id;
    std::size_t frame_buffer_size = 0U;
};

/// Admits a path and AVI, computes the bounded frame buffer, and creates a session ID.
PreviewOpenDecision decide_preview_open(std::string_view path,
                                        const core::AviPreview* avi,
                                        bool file_exists,
                                        bool file_opened,
                                        std::uint32_t random_high,
                                        std::uint32_t random_low);

}  // namespace firmware::application
