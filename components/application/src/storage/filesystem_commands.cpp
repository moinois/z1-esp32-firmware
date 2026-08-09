/** @file @brief Implements primary filesystem mutations, best-effort cache effects, and replies. */
#include "application/storage/filesystem_commands.hpp"

#include "core/filesystem/file_transfer_paths.hpp"
#include "core/filesystem/filesystem_syntax.hpp"
#include "core/protocol/protocol_constants.hpp"
#include "core/filesystem/sd_user_path.hpp"

#include <optional>
#include <string>
#include <utility>

namespace firmware::application {
namespace {

constexpr std::uint32_t directory_mode = 0777U;
constexpr std::string_view success_message = "ok\r\n";
constexpr std::string_view file_type_message = "ftype = nc\r\n";

// Creates one frame from text without exposing response construction at call sites.
core::Frame text_frame(std::uint8_t type, std::string text) {
    return {type, {text.begin(), text.end()}};
}

// Attempts creation of each mapped cache directory without changing the result.
void create_cache_directories(std::string_view path,
                              FilesystemCommandPort& port) {
    const core::FileCachePaths cache = core::map_file_cache_paths(path);
    if (cache.md5_path.has_value()) {
        static_cast<void>(port.create_directory(*cache.md5_path, directory_mode));
    }
    if (cache.compressed_path.has_value()) {
        static_cast<void>(port.create_directory(*cache.compressed_path,
                                                directory_mode));
    }
}

// Attempts removal of each mapped cache entry without changing the result.
void remove_cache_entries(std::string_view path, FilesystemCommandPort& port) {
    const core::FileCachePaths cache = core::map_file_cache_paths(path);
    if (cache.md5_path.has_value()) {
        port.remove_recursively(*cache.md5_path);
    }
    if (cache.compressed_path.has_value()) {
        port.remove_recursively(*cache.compressed_path);
    }
}

// Attempts a cache rename when both source and destination mappings exist.
void rename_cache_entry(const std::optional<std::string>& source,
                        const std::optional<std::string>& destination,
                        FilesystemCommandPort& port) {
    if (source.has_value() && destination.has_value()) {
        static_cast<void>(port.rename_path(*source, *destination));
    }
}

}  // namespace

void FilesystemCommands::make_directory(core::BytesView argument,
                                        FilesystemCommandPort& port) {
    const auto path = core::parse_filesystem_path(argument);
    if (!path.has_value()) {
        return;
    }
    const std::string displayed = core::logical_sd_path(*path);
    if (!port.create_directory(*path, directory_mode)) {
        port.send(text_frame(core::protocol::operation_failure,
                             "could not create directory " + displayed + "\r\n"));
        return;
    }

    port.send(text_frame(core::protocol::operation_success,
                         std::string(success_message)));
    port.send(text_frame(core::protocol::console_message,
                         "created directory " + displayed + "\r\n"));
    create_cache_directories(*path, port);
}

void FilesystemCommands::remove(core::BytesView argument,
                                FilesystemCommandPort& port) {
    const auto path = core::parse_remove_path(argument);
    if (!path.has_value()) {
        return;
    }
    const std::string displayed = core::logical_sd_path(*path);
    port.remove_recursively(*path);
    if (port.path_exists(*path)) {
        port.send(text_frame(core::protocol::operation_failure,
                             "Could not delete " + displayed + " \r\n"));
        return;
    }

    remove_cache_entries(*path, port);
    port.send(text_frame(core::protocol::operation_success,
                         std::string(success_message)));
}

void FilesystemCommands::move(core::BytesView argument,
                              FilesystemCommandPort& port) {
    const auto paths = core::parse_move_paths(argument);
    if (!paths.has_value()) {
        return;
    }
    const std::string displayed_source = core::logical_sd_path(paths->source);
    const std::string displayed_destination =
        core::logical_sd_path(paths->destination);
    if (!port.rename_path(paths->source, paths->destination)) {
        port.send(text_frame(core::protocol::operation_failure,
                             "Could not rename " + displayed_source + " to " +
                                 displayed_destination + "\r\n"));
        return;
    }

    const core::FileCachePaths source_cache =
        core::map_file_cache_paths(paths->source);
    const core::FileCachePaths destination_cache =
        core::map_file_cache_paths(paths->destination);
    rename_cache_entry(source_cache.md5_path, destination_cache.md5_path, port);
    rename_cache_entry(source_cache.compressed_path,
                       destination_cache.compressed_path, port);

    port.send(text_frame(core::protocol::operation_success,
                         std::string(success_message)));
    port.send(text_frame(core::protocol::console_message,
                         "renamed " + displayed_source + " to " +
                             displayed_destination + "\r\n"));
}

void FilesystemCommands::file_type(FilesystemCommandPort& port) {
    port.send(text_frame(core::protocol::console_message,
                         std::string(file_type_message)));
}

}  // namespace firmware::application
