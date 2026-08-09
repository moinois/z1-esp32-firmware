/** @file @brief Implements bounded RIFF chunk scanning, idx1 retention, and JPEG frame reads. */
#include "core/media/avi_preview.hpp"

#include <algorithm>
#include <string_view>

namespace firmware::core {
namespace {

constexpr std::size_t minimum_avi_size = 32U;
constexpr std::size_t riff_header_size = 12U;
constexpr std::size_t chunk_header_size = 8U;
constexpr std::size_t index_entry_size = 16U;

// Reads one little-endian unsigned 32-bit value when four bytes remain.
std::optional<std::uint32_t> read_u32(BytesView file, std::size_t offset,
                                      std::size_t limit) {
    if (offset > limit || limit - offset < 4U || offset > file.size() ||
        file.size() - offset < 4U) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(file[offset]) |
           (static_cast<std::uint32_t>(file[offset + 1U]) << 8U) |
           (static_cast<std::uint32_t>(file[offset + 2U]) << 16U) |
           (static_cast<std::uint32_t>(file[offset + 3U]) << 24U);
}

// Compares four bytes with one literal chunk identifier.
bool has_id(BytesView file, std::size_t offset, std::string_view id,
            std::size_t limit) {
    if (id.size() != 4U || offset > limit || limit - offset < 4U ||
        offset > file.size() || file.size() - offset < 4U) {
        return false;
    }
    for (std::size_t index = 0U; index < 4U; ++index) {
        if (file[offset + index] != static_cast<std::uint8_t>(id[index])) {
            return false;
        }
    }
    return true;
}

// Scans nested header chunks and retains the last readable avih/strf values.
bool scan_header_list(BytesView file, std::size_t start, std::size_t end,
                      AviPreview& result) {
    std::size_t cursor = start;
    while (cursor < end) {
        if (end - cursor < chunk_header_size) {
            return false;
        }
        const auto size = read_u32(file, cursor + 4U, end);
        if (!size.has_value() || *size > end - cursor - chunk_header_size) {
            return false;
        }
        const std::size_t data = cursor + chunk_header_size;
        const std::size_t next = data + *size + (*size & 1U);
        if (next > end) {
            return false;
        }
        if (has_id(file, cursor, "avih", end) && *size >= 4U) {
            result.frame_period_us = read_u32(file, data, end).value_or(0U);
        } else if (has_id(file, cursor, "strf", end) && *size >= 20U &&
                   read_u32(file, data, end).value_or(0U) >= 20U) {
            result.width = read_u32(file, data + 4U, end).value_or(0U);
            result.height = read_u32(file, data + 8U, end).value_or(0U);
        }
        cursor = next;
    }
    return cursor == end;
}

// Replaces the retained index after validating every idx1 extent and entry.
bool parse_index(BytesView file, std::size_t data, std::uint32_t size,
                 std::size_t extent, AviPreview& result) {
    if (size == 0U || size % index_entry_size != 0U ||
        size > extent - data) {
        return false;
    }
    std::vector<AviIndexEntry> entries;
    entries.reserve(size / index_entry_size);
    for (std::size_t cursor = data; cursor < data + size;
         cursor += index_entry_size) {
        const auto offset = read_u32(file, cursor + 8U, data + size);
        const auto advertised = read_u32(file, cursor + 12U, data + size);
        if (!offset.has_value() || !advertised.has_value()) {
            return false;
        }
        entries.push_back({*offset, *advertised});
    }
    result.entries = std::move(entries);
    return true;
}

}  // namespace

std::optional<AviPreview> AviPreview::parse(BytesView file) {
    if (file.size() < minimum_avi_size || !has_id(file, 0U, "RIFF", file.size()) ||
        !has_id(file, 8U, "AVI ", file.size())) {
        return std::nullopt;
    }
    const auto riff_size = read_u32(file, 4U, file.size());
    if (!riff_size.has_value() || *riff_size > file.size() - 8U) {
        return std::nullopt;
    }
    const std::size_t extent = static_cast<std::size_t>(*riff_size) + 8U;
    AviPreview result;
    std::size_t cursor = riff_header_size;
    while (cursor < extent) {
        if (extent - cursor < chunk_header_size) {
            return std::nullopt;
        }
        const auto size = read_u32(file, cursor + 4U, extent);
        if (!size.has_value() || *size > extent - cursor - chunk_header_size) {
            return std::nullopt;
        }
        const std::size_t data = cursor + chunk_header_size;
        const std::size_t next = data + *size + (*size & 1U);
        if (next > extent) {
            return std::nullopt;
        }
        if (has_id(file, cursor, "LIST", extent) && *size >= 4U &&
            has_id(file, data, "hdrl", extent)) {
            if (!scan_header_list(file, data + 4U, data + *size, result)) {
                return std::nullopt;
            }
        } else if (has_id(file, cursor, "LIST", extent) && *size >= 4U &&
                   has_id(file, data, "movi", extent)) {
            result.movi_data_offset = data + 4U;
        } else if (has_id(file, cursor, "idx1", extent) &&
                   !parse_index(file, data, *size, extent, result)) {
            return std::nullopt;
        }
        cursor = next;
    }
    if (result.frame_period_us == 0U) {
        result.frame_period_us = 100000U;
    }
    if (result.movi_data_offset == 0U || result.entries.empty()) {
        return std::nullopt;
    }
    return result;
}

std::optional<ByteVector> read_avi_frame(BytesView file, const AviPreview& avi,
                                          std::size_t index,
                                          std::size_t frame_buffer_size) {
    if (index >= avi.entries.size() || avi.movi_data_offset < 4U) {
        return std::nullopt;
    }
    const AviIndexEntry& entry = avi.entries[index];
    if (entry.advertised_size == 0U ||
        entry.advertised_size > frame_buffer_size ||
        avi.movi_data_offset > file.size() ||
        entry.offset > file.size() - avi.movi_data_offset + 4U) {
        return std::nullopt;
    }
    const std::size_t seek = avi.movi_data_offset + entry.offset - 4U;
    if (seek > file.size() || file.size() - seek < chunk_header_size ||
        !has_id(file, seek, "00dc", file.size())) {
        return std::nullopt;
    }
    const auto actual_size = read_u32(file, seek + 4U, file.size());
    if (!actual_size.has_value() || *actual_size == 0U ||
        *actual_size > frame_buffer_size ||
        *actual_size > file.size() - seek - chunk_header_size) {
        return std::nullopt;
    }
    const std::size_t payload = seek + chunk_header_size;
    return ByteVector(file.begin() + static_cast<std::ptrdiff_t>(payload),
                      file.begin() + static_cast<std::ptrdiff_t>(payload + *actual_size));
}

}  // namespace firmware::core
