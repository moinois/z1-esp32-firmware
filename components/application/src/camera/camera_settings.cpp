/** @file @brief Implements strict signed-decimal camera configuration and dimension policy. */
#include "application/camera/camera_settings.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <limits>
#include <string_view>

namespace firmware::application {
namespace {

constexpr std::string_view stream_size_key =
    camera_stream_frame_size_key;
constexpr std::string_view recording_size_key =
    camera_recording_frame_size_key;
constexpr std::string_view frame_interval_key =
    camera_frame_interval_key;
constexpr std::string_view frames_per_file_key =
    camera_frames_per_file_key;
constexpr std::uint64_t minimum_frame_interval_milliseconds = 1000U;
constexpr std::size_t maximum_parsed_value_size = 31U;

constexpr std::array<FrameDimensions, maximum_camera_frame_size> frame_dimensions{{
    {160U, 120U},
    {128U, 128U},
    {176U, 144U},
    {240U, 176U},
    {240U, 240U},
    {320U, 240U},
    {320U, 320U},
    {400U, 296U},
    {480U, 320U},
    {640U, 480U},
    {800U, 600U},
    {1024U, 768U},
    {1280U, 720U},
    {1280U, 1024U},
    {1600U, 1200U},
}};

bool whitespace(char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n' ||
           value == '\f' || value == '\v';
}

struct DecimalResult {
    bool has_digits = false;
    std::int32_t value = 0;
    std::string_view suffix;
};

// Mirrors the bounded signed-32 conversion used by the normative camera parser.
DecimalResult signed_decimal(std::string_view source) {
    const std::string_view text = source.substr(
        0U, std::min(source.size(), maximum_parsed_value_size));
    if (text.empty()) return {};
    std::string_view digits = text;
    if (digits.front() == '+') {
        digits.remove_prefix(1U);
        if (digits.empty()) return {};
    }
    std::int32_t value = 0;
    const auto parsed = std::from_chars(digits.data(),
                                        digits.data() + digits.size(),
                                        value,
                                        10);
    if (parsed.ptr == digits.data()) return {};
    if (parsed.ec == std::errc::result_out_of_range) {
        value = text.front() == '-'
                    ? std::numeric_limits<std::int32_t>::min()
                    : std::numeric_limits<std::int32_t>::max();
    }
    return {true, value,
            std::string_view(parsed.ptr,
                             static_cast<std::size_t>(digits.data() + digits.size() -
                                                      parsed.ptr))};
}

// Normalizes a syntactically valid frame-size value to the accepted range.
std::uint8_t normalized_frame_size(std::int32_t value) {
    if (value < minimum_camera_frame_size || value > maximum_camera_frame_size) {
        return default_camera_frame_size;
    }
    return static_cast<std::uint8_t>(value);
}

std::optional<std::int32_t> parsed_setting(
    std::string_view text, bool frames_setting,
    const CameraConfigSource& source) {
    const DecimalResult parsed = signed_decimal(text);
    if (!parsed.has_digits) {
        source.report_conversion_diagnostic(
            frames_setting ? CameraConversionDiagnostic::frames_has_no_digits
                           : CameraConversionDiagnostic::value_has_no_digits,
            {});
        return std::nullopt;
    }
    if (!parsed.suffix.empty()) {
        source.report_conversion_diagnostic(
            frames_setting ? CameraConversionDiagnostic::frames_has_suffix
                           : CameraConversionDiagnostic::value_has_suffix,
            parsed.suffix);
        return std::nullopt;
    }
    return parsed.value;
}

}  // namespace

std::optional<std::string_view> camera_value_token_from_chunk(
    std::string_view chunk, std::string_view key) {
    if (chunk.size() < key.size() || chunk.substr(0U, key.size()) != key) {
        return std::nullopt;
    }
    chunk.remove_prefix(key.size());
    if (!chunk.empty() && !whitespace(chunk.front()) && chunk.front() != '=') {
        return std::nullopt;
    }
    while (!chunk.empty() && (whitespace(chunk.front()) || chunk.front() == '=')) {
        chunk.remove_prefix(1U);
    }
    const auto end = std::find_if(chunk.begin(), chunk.end(), whitespace);
    if (end == chunk.begin()) return std::nullopt;
    return chunk.substr(0U, static_cast<std::size_t>(end - chunk.begin()));
}

const CameraSettings& CameraSettingsLoader::load_once(
    const CameraConfigSource& source) {
    if (loaded_) {
        return settings_;
    }
    loaded_ = true;

    if (const auto text = source.find(stream_size_key); text.has_value()) {
        if (const auto value = parsed_setting(*text, false, source); value.has_value()) {
            settings_.stream_frame_size = normalized_frame_size(*value);
        }
    }
    if (const auto text = source.find(recording_size_key); text.has_value()) {
        if (const auto value = parsed_setting(*text, false, source); value.has_value()) {
            settings_.recording_frame_size = normalized_frame_size(*value);
        }
    }
    if (const auto text = source.find(frame_interval_key); text.has_value()) {
        if (const auto value = parsed_setting(*text, false, source); value.has_value()) {
            settings_.frame_interval_milliseconds =
                *value < static_cast<std::int64_t>(
                             minimum_frame_interval_milliseconds)
                    ? minimum_frame_interval_milliseconds
                    : static_cast<std::uint64_t>(*value);
        }
    }
    if (const auto text = source.find(frames_per_file_key); text.has_value()) {
        if (const auto value = parsed_setting(*text, true, source); value.has_value()) {
            settings_.frames_per_file = static_cast<std::uint32_t>(*value);
        }
    }
    return settings_;
}

FrameDimensions camera_dimensions(std::uint8_t frame_size) {
    if (frame_size < minimum_camera_frame_size ||
        frame_size > maximum_camera_frame_size) {
        return fallback_camera_dimensions;
    }
    return frame_dimensions[frame_size - minimum_camera_frame_size];
}

}  // namespace firmware::application
