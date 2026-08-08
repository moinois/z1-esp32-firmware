/** @file @brief Implements nonfatal web-volume mount and format-on-failure sequencing. */
#include "firmware/application/web_volume_startup.hpp"

namespace firmware::application {

bool WebVolumeStartup::start(WebVolumePort& port) const {
    if (port.mount(web_volume)) {
        return true;
    }
    if (!web_volume.format_if_mount_fails || !port.format(web_volume)) {
        return false;
    }
    return port.mount(web_volume);
}

}  // namespace firmware::application
