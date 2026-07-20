// Declares one-shot camera configuration loading and frame-size mapping.
#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace firmware::application {

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
