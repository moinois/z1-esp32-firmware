/** @file @brief Declares nonfatal web-volume mounting and format-on-failure orchestration. */
#pragma once

#include "application/web/web_config.hpp"

namespace firmware::application {

/// Isolates SPIFFS mount and formatting side effects from startup policy.
class WebVolumePort {
public:
    /// Enables safe destruction through a substituted volume adapter.
    virtual ~WebVolumePort() = default;

    /// Attempts to mount the configured web volume.
    virtual bool mount(const WebVolumeConfig& config) = 0;

    /// Formats the configured volume and reports whether formatting succeeded.
    virtual bool format(const WebVolumeConfig& config) = 0;
};

/// Applies one mount, optional format, and one retry without making failure fatal.
class WebVolumeStartup {
public:
    /// Starts the volume and reports whether it is mounted after the retry.
    bool start(WebVolumePort& port) const;
};

}  // namespace firmware::application
