/** @file @brief Implements centralized paths for firmware-owned SD content. */
#include "core/filesystem/sd_user_path.hpp"

#include <string>
#include <string_view>

namespace firmware::core {
std::string physical_sd_path(std::string_view logical_path) {
    if (logical_path.empty() || logical_path == "/") {
        return std::string(sd_mount_path);
    }
    std::string result(sd_mount_path);
    if (logical_path.front() != '/') result.push_back('/');
    result.append(logical_path);
    return result;
}

}  // namespace firmware::core
