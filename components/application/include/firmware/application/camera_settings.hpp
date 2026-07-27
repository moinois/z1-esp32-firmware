// Declares one-shot camera configuration loading and frame-size mapping.
#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace firmware::application {

// Groups all camera settings under the CAMERA_ namespace in config.txt.
inline constexpr std::string_view camera_configuration_tag = "CAMERA";
inline constexpr std::string_view camera_stream_frame_size_key =
    "video_stream_framesize";
inline constexpr std::string_view camera_recording_frame_size_key =
    "video_rec_framesize";
inline constexpr std::string_view camera_frame_interval_key =
    "Time_interval_frames";
inline constexpr std::string_view camera_frames_per_file_key =
    "frames_of_one_file";

// Holds pixel dimensions corresponding to one camera frame-size number.
struct FrameDimensions {
    std::uint16_t width;
    std::uint16_t height;

    // Supports exact dimension comparisons in policy tests and adapters.
    bool operator==(const FrameDimensions& other) const {
        return width == other.width && height == other.height;
    }
};

inline constexpr FrameDimensions fallback_camera_dimensions{1600U, 1200U};
inline constexpr std::uint8_t minimum_camera_frame_size = 1U;
inline constexpr std::uint8_t maximum_camera_frame_size = 15U;
inline constexpr std::uint8_t default_camera_frame_size = 10U;

// Holds all normalized camera and recording configuration values.
struct CameraSettings {
    std::uint8_t stream_frame_size = 10U;
    std::uint8_t recording_frame_size = 15U;
    std::uint64_t frame_interval_milliseconds = 1000U;
    std::uint64_t frames_per_file = 300U;
};

// Isolates camera settings from the SD configuration storage implementation.
class CameraConfigSource {
public:
    // Enables safe destruction through a substituted configuration source.
    virtual ~CameraConfigSource() = default;

    // Returns the first retained value for one exact configuration key.
    virtual std::optional<std::string_view> find(
        std::string_view key) const = 0;
};

// Loads and normalizes camera settings at most once before mainboard reset.
class CameraSettingsLoader {
public:
    // Returns the cached settings, loading all four keys on the first call.
    const CameraSettings& load_once(const CameraConfigSource& source);

private:
    CameraSettings settings_;
    bool loaded_ = false;
};

// Maps one normalized frame-size number to its exact pixel dimensions.
FrameDimensions camera_dimensions(std::uint8_t frame_size);

}  // namespace firmware::application
