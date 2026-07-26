// Tests one-shot camera setting loading, normalization, and frame dimensions.
#include "test.hpp"

#include "firmware/application/camera_settings.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

using firmware::application::CameraConfigSource;
using firmware::application::CameraSettingsLoader;
using firmware::application::FrameDimensions;

namespace {

class FakeCameraConfigSource final : public CameraConfigSource {
public:
    // Returns the retained first configuration value for one exact key.
    std::optional<std::string_view> find(std::string_view key) const override {
        ++find_count;
        const auto found = values.find(std::string(key));
        if (found == values.end()) {
            return std::nullopt;
        }
        return found->second;
    }

    std::unordered_map<std::string, std::string> values;
    mutable std::size_t find_count = 0U;
};

}  // namespace

TEST_CASE(cam_001_missing_and_invalid_values_keep_field_specific_defaults) {
    FakeCameraConfigSource source;
    source.values["video_stream_framesize"] = "10x";
    source.values["video_rec_framesize"] = "";
    source.values["Time_interval_frames"] = "+";
    source.values["frames_of_one_file"] = " 300";
    CameraSettingsLoader loader;

    const auto& settings = loader.load_once(source);

    REQUIRE_EQ(settings.stream_frame_size, 10U);
    REQUIRE_EQ(settings.recording_frame_size, 15U);
    REQUIRE_EQ(settings.frame_interval_milliseconds, 1000U);
    REQUIRE_EQ(settings.frames_per_file, 300U);
}

TEST_CASE(cam_001_complete_values_are_normalized_by_each_setting_rule) {
    FakeCameraConfigSource source;
    source.values["video_stream_framesize"] = "16";
    source.values["video_rec_framesize"] = "0";
    source.values["Time_interval_frames"] = "250";
    source.values["frames_of_one_file"] = "0";
    CameraSettingsLoader loader;

    const auto& settings = loader.load_once(source);

    REQUIRE_EQ(settings.stream_frame_size, 10U);
    REQUIRE_EQ(settings.recording_frame_size, 10U);
    REQUIRE_EQ(settings.frame_interval_milliseconds, 1000U);
    REQUIRE_EQ(settings.frames_per_file, 0U);
}

TEST_CASE(cam_001_negative_frame_limit_keeps_default_but_large_values_work) {
    FakeCameraConfigSource negative;
    negative.values["frames_of_one_file"] = "-1";
    CameraSettingsLoader negative_loader;
    REQUIRE_EQ(negative_loader.load_once(negative).frames_per_file, 300U);

    FakeCameraConfigSource valid;
    valid.values["Time_interval_frames"] = "2500";
    valid.values["frames_of_one_file"] = "4294967295";
    CameraSettingsLoader valid_loader;
    REQUIRE_EQ(valid_loader.load_once(valid).frame_interval_milliseconds,
               2500U);
    REQUIRE_EQ(valid_loader.load_once(valid).frames_per_file, 4294967295U);
}

TEST_CASE(cam_002_all_frame_size_numbers_map_to_exact_dimensions) {
    constexpr FrameDimensions expected[] = {
        {160U, 120U}, {128U, 128U}, {176U, 144U}, {240U, 176U},
        {240U, 240U}, {320U, 240U}, {320U, 320U}, {400U, 296U},
        {480U, 320U}, {640U, 480U}, {800U, 600U}, {1024U, 768U},
        {1280U, 720U}, {1280U, 1024U}, {1600U, 1200U}};

    for (std::uint8_t value = 1U; value <= 15U; ++value) {
        REQUIRE_EQ(firmware::application::camera_dimensions(value),
                   expected[value - 1U]);
    }
}

TEST_CASE(cam_005_unavailable_dimensions_use_uxga_fallback) {
    REQUIRE_EQ(firmware::application::fallback_camera_dimensions,
               FrameDimensions({1600U, 1200U}));
}

TEST_CASE(cam_008_settings_are_loaded_only_once_per_loader_lifetime) {
    FakeCameraConfigSource source;
    source.values["video_stream_framesize"] = "9";
    CameraSettingsLoader loader;
    REQUIRE_EQ(loader.load_once(source).stream_frame_size, 9U);
    const std::size_t initial_reads = source.find_count;
    source.values["video_stream_framesize"] = "4";

    REQUIRE_EQ(loader.load_once(source).stream_frame_size, 9U);
    REQUIRE_EQ(source.find_count, initial_reads);
}
