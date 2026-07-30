// Implements shared camera configuration loading through the SD file store.
#include "camera_settings_adapter.hpp"

#include "configuration_file_store.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace firmware::target {
namespace {

// Exposes the SD configuration file through the portable camera source port.
class SdCameraConfigSource final
    : public firmware::application::CameraConfigSource {
public:
    // Finds one camera key while retaining returned string-view storage.
    std::optional<std::string_view> find(std::string_view key) const override {
        const auto value = ConfigurationFileStore{}.get(
            firmware::application::camera_configuration_tag, key);
        if (!value.has_value()) return std::nullopt;
        values_.emplace_back(*value);
        return values_.back();
    }

private:
    mutable std::vector<std::string> values_;
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
