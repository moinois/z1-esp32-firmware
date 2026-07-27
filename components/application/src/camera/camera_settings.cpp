// Implements strict signed-decimal camera configuration and dimension policy.
#include "firmware/application/camera_settings.hpp"

#include <array>
#include <charconv>
#include <cstdint>
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

// Parses a complete signed decimal, including an explicit positive sign.
std::optional<std::int64_t> signed_decimal(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }
    bool positive_sign = text.front() == '+';
    if (positive_sign) {
        text.remove_prefix(1U);
        if (text.empty()) {
            return std::nullopt;
        }
    }
    std::int64_t value = 0;
    const auto parsed = std::from_chars(text.data(),
                                        text.data() + text.size(),
                                        value,
                                        10);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

// Normalizes a syntactically valid frame-size value to the accepted range.
std::uint8_t normalized_frame_size(std::int64_t value) {
    if (value < minimum_camera_frame_size || value > maximum_camera_frame_size) {
        return default_camera_frame_size;
    }
    return static_cast<std::uint8_t>(value);
}

}  // namespace

const CameraSettings& CameraSettingsLoader::load_once(
    const CameraConfigSource& source) {
    if (loaded_) {
        return settings_;
    }
    loaded_ = true;

    if (const auto text = source.find(stream_size_key); text.has_value()) {
        if (const auto value = signed_decimal(*text); value.has_value()) {
            settings_.stream_frame_size = normalized_frame_size(*value);
        }
    }
    if (const auto text = source.find(recording_size_key); text.has_value()) {
        if (const auto value = signed_decimal(*text); value.has_value()) {
            settings_.recording_frame_size = normalized_frame_size(*value);
        }
    }
    if (const auto text = source.find(frame_interval_key); text.has_value()) {
        if (const auto value = signed_decimal(*text); value.has_value()) {
            settings_.frame_interval_milliseconds =
                *value < static_cast<std::int64_t>(
                             minimum_frame_interval_milliseconds)
                    ? minimum_frame_interval_milliseconds
                    : static_cast<std::uint64_t>(*value);
        }
    }
    if (const auto text = source.find(frames_per_file_key); text.has_value()) {
        if (const auto value = signed_decimal(*text);
            value.has_value() && *value >= 0) {
            settings_.frames_per_file = static_cast<std::uint64_t>(*value);
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
