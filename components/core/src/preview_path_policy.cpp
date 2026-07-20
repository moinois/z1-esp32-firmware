// Implements preview path root matching and literal traversal rejection.
#include "firmware/core/preview_path_policy.hpp"

namespace firmware::core {

bool preview_path_allowed(std::string_view path) {
    constexpr std::string_view videos_root = "/sd/videos";
    if (path != videos_root &&
        (path.size() <= videos_root.size() ||
         path.substr(0U, videos_root.size()) != videos_root ||
         path[videos_root.size()] != '/')) {
        return false;
    }
    return path.find("..") == std::string_view::npos;
}

}  // namespace firmware::core
