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

// Holds normalized source and destination paths for one move operation.
struct MovePaths {
    std::string source;
    std::string destination;
};

// Decodes and normalizes one common filesystem path argument.
std::optional<std::string> parse_filesystem_path(BytesView argument);

// Consumes bounded leading list options and parses the remaining path.
std::optional<DirectoryListArguments> parse_directory_list_arguments(
    BytesView argument);

// Consumes an optional recursive flag and normalizes the remove path.
std::optional<std::string> parse_remove_path(BytesView argument);

// Splits and normalizes the two paths accepted by a move command.
std::optional<MovePaths> parse_move_paths(BytesView argument);

}  // namespace firmware::core
