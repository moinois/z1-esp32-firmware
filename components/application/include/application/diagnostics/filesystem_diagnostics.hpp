/** @file @brief Declares exact DIAG-028 POSIX filesystem formatting. */
#pragma once

#include <string>
#include <string_view>

namespace firmware::application {

std::string filesystem_opendir_failure(std::string_view path, int error);
std::string filesystem_mkdir_failure(std::string_view path, int error);
std::string filesystem_remove_failure(std::string_view path, int result,
                                      int error);
std::string filesystem_rename_failure(std::string_view source,
                                      std::string_view destination, int error);

}  // namespace firmware::application
