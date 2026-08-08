/** @file @brief Declares the ESP-IDF SPIFFS adapter for the portable web-volume startup policy. */
#pragma once

#include "firmware/application/web_volume_startup.hpp"

namespace firmware::target {

/// Translates web-volume mount and formatting requests to ESP-IDF SPIFFS calls.
class WebVolumeAdapter final : public application::WebVolumePort {
public:
    /// Registers the SPIFFS VFS with the requested mount configuration.
    bool mount(const application::WebVolumeConfig& config) override;

    /// Unregisters the failed volume and formats its SPIFFS partition.
    bool format(const application::WebVolumeConfig& config) override;
};

}  // namespace firmware::target
