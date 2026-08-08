/** @file @brief Declares target configuration loading shared by live and mock cameras. */
#pragma once

#include "firmware/application/camera_settings.hpp"

namespace firmware::target {

/// Loads and retains normalized camera settings from the SD configuration file.
const firmware::application::CameraSettings& load_camera_settings();

}  // namespace firmware::target
