/** @file
 *  @brief Central mount-point definitions for firmware-owned SD content.
 */
#pragma once

#include <string>
#include <string_view>

namespace firmware::core {

/// Physical VFS mount point used by requirements that explicitly name SD data.
inline constexpr std::string_view sd_mount_path = "/sd";
/// Mount prefix used when joining a physical path below the SD root.
inline constexpr std::string_view sd_mount_prefix = "/sd/";

/** Builds a physical path for trusted firmware-owned SD content.
 *  This helper does not accept general host paths. User-controlled paths are
 *  normalized according to HFT-004 and retain their resolved filesystem root.
 *  @param logical_path Path relative to the SD mount point.
 *  @return Absolute VFS path rooted at @ref sd_mount_path.
 */
std::string physical_sd_path(std::string_view logical_path);

}  // namespace firmware::core
