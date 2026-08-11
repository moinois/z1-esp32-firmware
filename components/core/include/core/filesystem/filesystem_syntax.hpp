/** @file
 *  @brief Transport-independent parsing for path-oriented host commands.
 */
#pragma once

#include "core/protocol/bytes.hpp"

#include <optional>
#include <string>

namespace firmware::core {

/** Normalized arguments for a directory-listing request. */
struct DirectoryListArguments {
    /// User-visible path after option removal and lexical normalization.
    std::string path;
    /// Whether the response must include metadata rather than names only.
    bool include_details = false;
};

/** Normalized source and destination for one atomic move request. */
struct MovePaths {
    /// Existing user-visible path.
    std::string source;
    /// Requested user-visible destination path.
    std::string destination;
};

/// Distinguishes the two malformed `mv` forms with normative diagnostics.
enum class MovePathError {
    none,
    missing_separator,
    empty_path,
};

/// Carries parsed move paths or the exact syntax category that prevented them.
struct MovePathParseResult {
    std::optional<MovePaths> paths;
    MovePathError error = MovePathError::none;
};

/** Decodes escapes and lexically normalizes one filesystem argument.
 *  @param argument Raw command argument bytes.
 *  @return Normalized path, or no value when syntax or bounds are invalid.
 */
std::optional<std::string> parse_filesystem_path(BytesView argument);

/** Parses bounded leading `ls` options without changing any working directory.
 *  @param argument Raw bytes following the command token.
 *  @return Parsed listing arguments, or no value for malformed input.
 */
std::optional<DirectoryListArguments> parse_directory_list_arguments(
    BytesView argument);

/** Parses an optional recursive flag and the following remove path.
 *  @param argument Raw bytes following the remove command.
 *  @return Normalized target path, or no value for malformed input.
 */
std::optional<std::string> parse_remove_path(BytesView argument);

/** Splits and normalizes the two paths accepted by a move command.
 *  @param argument Raw bytes following the move command.
 *  @return Both paths, or no value unless exactly two valid paths are present.
 */
std::optional<MovePaths> parse_move_paths(BytesView argument);

/** Parses move paths while preserving the diagnostic reason for failure. */
MovePathParseResult parse_move_paths_detailed(BytesView argument);

}  // namespace firmware::core
