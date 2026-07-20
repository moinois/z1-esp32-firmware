// Declares RIFF/AVI preview acceptance and indexed JPEG frame reads.
#pragma once

#include "firmware/core/bytes.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace firmware::core {

// Stores one AVI idx1 entry needed for a later frame seek.
struct AviIndexEntry {
    std::uint32_t offset = 0U;
    std::uint32_t advertised_size = 0U;
};

// Holds the accepted AVI metadata and last retained index/movi pair.
struct AviPreview {
    std::uint32_t frame_period_us = 100000U;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    std::size_t movi_data_offset = 0U;
    std::vector<AviIndexEntry> entries;

    // Parses one complete file according to the AVI preview acceptance rules.
    static std::optional<AviPreview> parse(BytesView file);
};

// Reads one indexed 00dc frame into a bounded preview buffer.
std::optional<ByteVector> read_avi_frame(BytesView file, const AviPreview& avi,
                                          std::size_t index,
                                          std::size_t frame_buffer_size);

}  // namespace firmware::core
