/** @file
 *  @brief Canonical sandboxing of user-controlled paths inside the SD volume.
 */
#pragma once

#include <string>
#include <string_view>

namespace firmware::core {

/// Physical VFS mount point; user-facing commands still see `/` as their root.
inline constexpr std::string_view sd_mount_path = "/sd";
/// Mount prefix used when joining a physical path below the SD root.
inline constexpr std::string_view sd_mount_prefix = "/sd/";

/** Builds a physical path for trusted firmware-owned logical content.
 *  @param logical_path Path expressed in the user-visible SD namespace.
 *  @return Absolute VFS path rooted at @ref sd_mount_path.
 */
std::string physical_sd_path(std::string_view logical_path);

/** Resolves untrusted user input without permitting escape into another VFS.
 *  Compatibility input beginning with `/sd` is treated as the same logical
 *  root, and parent components are confined rather than allowed to escape.
 *  @param path User-supplied logical or compatibility path.
 *  @return Absolute physical path guaranteed to remain beneath `/sd`.
 */
std::string resolve_sd_user_path(std::string_view path);

/** Removes the private mount prefix from a user-friendly response path.
 *  This is presentation-only: callers must first resolve and access the
 *  physical path with @ref resolve_sd_user_path. Normative protocol responses
 *  that require a resolved path (for example FILE-020, FILE-028, or FILE-029)
 *  must return the physical value directly instead. A future UI-oriented
 *  response can call this helper after the operation has completed.
 *  @param physical_path Path returned by an internal SD operation.
 *  @return Path in the user-visible namespace rooted at `/`.
 */
std::string logical_sd_path(std::string_view physical_path);

}  // namespace firmware::core
