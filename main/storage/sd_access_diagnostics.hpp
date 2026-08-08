/** @file @brief Declares shared target mount state and diagnostic reporting for SD failures. */
#pragma once

#include <string_view>

namespace firmware::target {

/// Publishes the last successfully established physical SD mount state.
void set_sd_storage_mounted(bool mounted);

/// Reports whether the physical SD volume is currently mounted.
bool sd_storage_mounted();

/// Logs one failed SD operation with a stable reason and numeric POSIX errno.
void log_sd_access_failure(std::string_view operation, std::string_view path,
                           int error_number);

}  // namespace firmware::target
