// Declares canonical resolution of user-controlled paths inside the SD volume.
#pragma once

#include <string>
#include <string_view>

namespace firmware::core {

inline constexpr std::string_view sd_mount_path = "/sd";
inline constexpr std::string_view sd_mount_prefix = "/sd/";

// Builds one fixed internal path beneath the centralized physical mount point.
std::string physical_sd_path(std::string_view logical_path);

// Resolves one normalized user path beneath /sd without permitting VFS escape.
std::string resolve_sd_user_path(std::string_view path);

// Converts a resolved physical SD path to the user-visible logical namespace.
std::string logical_sd_path(std::string_view physical_path);

}  // namespace firmware::core
