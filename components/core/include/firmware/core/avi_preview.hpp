/** @file @brief RIFF/AVI preview validation and indexed JPEG frame reads. */
#pragma once

#include "firmware/core/bytes.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace firmware::core {

/** Minimal validated `idx1` entry needed for a later frame seek. */
struct AviIndexEntry {
    /// Chunk offset interpreted relative to the retained `movi` data origin.
    std::uint32_t offset = 0U;
    /// Encoded chunk size used to bound and validate the read.
    std::uint32_t advertised_size = 0U;
};

/** Accepted AVI metadata and the last matching `movi`/`idx1` pair. */
struct AviPreview {
    /// Presentation interval derived from stream timing, with safe fallback.
    std::uint32_t frame_period_us = 100000U;
    /// Declared frame width used for preview metadata.
    std::uint32_t width = 0U;
    /// Declared frame height used for preview metadata.
    std::uint32_t height = 0U;
    /// Absolute byte offset at which indexed `movi` data begins.
    std::size_t movi_data_offset = 0U;
    /// Validated MJPEG frame entries in playback order.
    std::vector<AviIndexEntry> entries;

    /** Parses a complete file according to the bounded preview acceptance rules. */
    static std::optional<AviPreview> parse(BytesView file);
};

/** Reads one indexed `00dc` frame into a bounded preview buffer.
 *  @return Exact JPEG bytes, or no value for bad index, bounds, tag, or size.
 */
std::optional<ByteVector> read_avi_frame(BytesView file, const AviPreview& avi,
                                          std::size_t index,
                                          std::size_t frame_buffer_size);

}  // namespace firmware::core
