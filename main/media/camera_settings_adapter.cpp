/** @file @brief Implements shared camera configuration loading through the SD file store. */
#include "camera_settings_adapter.hpp"

#include "configuration_file_store.hpp"

#include "esp_log.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

namespace firmware::target {
namespace {

constexpr char diagnostic_tag[] = "WEBSERVER";
constexpr std::size_t configuration_chunk_size = 255U;

// Exposes the SD configuration file through the portable camera source port.
class SdCameraConfigSource final
    : public firmware::application::CameraConfigSource {
public:
    // Replays the camera's 255-byte chunk scan and returns its first value token.
    std::optional<std::string_view> find(std::string_view key) const override {
        std::FILE* file = std::fopen(
            std::string(active_configuration_path()).c_str(), "rb");
        if (file == nullptr) return std::nullopt;
        std::array<char, configuration_chunk_size + 1U> chunk{};
        while (std::fgets(chunk.data(), static_cast<int>(chunk.size()), file) != nullptr) {
            std::string_view line(chunk.data());
            const auto token =
                firmware::application::camera_value_token_from_chunk(line, key);
            if (!token.has_value()) continue;
            value_.assign(*token);
            std::fclose(file);
            return value_;
        }
        std::fclose(file);
        return std::nullopt;
    }

    void report_conversion_diagnostic(
        firmware::application::CameraConversionDiagnostic diagnostic,
        std::string_view suffix) const override {
        using Diagnostic = firmware::application::CameraConversionDiagnostic;
        switch (diagnostic) {
            case Diagnostic::value_has_no_digits:
                ESP_LOGE(diagnostic_tag, "错误：没有数字被转换。");
                break;
            case Diagnostic::value_has_suffix:
                ESP_LOGE(diagnostic_tag, "警告：部分转换，非数字字符为：%.*s",
                         static_cast<int>(suffix.size()), suffix.data());
                break;
            case Diagnostic::frames_has_no_digits:
                ESP_LOGE(diagnostic_tag, "错误：没有数字被转换");
                break;
            case Diagnostic::frames_has_suffix:
                ESP_LOGW(diagnostic_tag, "警告：部分转换，非数字字符为：'%.*s'",
                         static_cast<int>(suffix.size()), suffix.data());
                break;
        }
    }

private:
    mutable std::string value_;
};

}  // namespace

const firmware::application::CameraSettings& load_camera_settings() {
    static const firmware::application::CameraSettings settings = [] {
        SdCameraConfigSource source;
        firmware::application::CameraSettingsLoader loader;
        return loader.load_once(source);
    }();
    return settings;
}

}  // namespace firmware::target
