/** @file @brief Implements preview path root matching and literal traversal rejection. */
#include "core/media/preview_path_policy.hpp"

#include "core/filesystem/sd_user_path.hpp"

namespace firmware::core {

bool preview_path_allowed(std::string_view path) {
    const std::string videos_root = physical_sd_path("/videos");
    if (path != videos_root &&
        (path.size() <= videos_root.size() ||
         path.substr(0U, videos_root.size()) != videos_root ||
         path[videos_root.size()] != '/')) {
        return false;
    }
    return path.find("..") == std::string_view::npos;
}

}  // namespace firmware::core
