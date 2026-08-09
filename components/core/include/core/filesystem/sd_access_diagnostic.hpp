/** @file @brief Stable diagnostic reasons for failed SD-root access. */
#pragma once

#include <string_view>

namespace firmware::core {

/** Classifies one failed SD operation, giving mount state priority over errno.
 *  @param mounted Whether the SD lifecycle currently exposes a mounted volume.
 *  @param error_number POSIX error captured immediately after the operation.
 *  @return Stable diagnostic text suitable for logs and automated tests.
 */
std::string_view sd_access_failure_reason(bool mounted, int error_number);

}  // namespace firmware::core
