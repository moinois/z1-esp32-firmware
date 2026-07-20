// Declares transport-independent parsing for path-oriented filesystem commands.
#pragma once

#include "firmware/core/bytes.hpp"

#include <optional>
#include <string>

namespace firmware::core {

// Holds the normalized path and output mode selected by an `ls` argument.
struct DirectoryListArguments {
    std::string path;
    bool include_details = false;
};

// Decodes and normalizes one common filesystem path argument.
std::optional<std::string> parse_filesystem_path(BytesView argument);

// Consumes bounded leading list options and parses the remaining path.
std::optional<DirectoryListArguments> parse_directory_list_arguments(
    BytesView argument);

}  // namespace firmware::core
