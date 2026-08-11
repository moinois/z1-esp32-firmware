/** @file @brief Declares shared POSIX filesystem mutations with DIAG-028 logs. */
#pragma once

#include <cstdint>
#include <string_view>

namespace firmware::target {

bool create_posix_directory(std::string_view path, std::uint32_t mode);
void remove_posix_tree(std::string_view path);
bool rename_posix_path(std::string_view source, std::string_view destination);

}  // namespace firmware::target
