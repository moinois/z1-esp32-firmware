// Declares filesystem mutation policy behind a replaceable storage port.
#pragma once

#include "firmware/core/bytes.hpp"
#include "firmware/core/frame.hpp"

#include <cstdint>
#include <string_view>

namespace firmware::application {

// Isolates command policy from target filesystem and response transport APIs.
class FilesystemCommandPort {
public:
    // Enables safe destruction through a substituted port implementation.
    virtual ~FilesystemCommandPort() = default;

    // Creates exactly one directory using the requested mode.
    virtual bool create_directory(std::string_view path, std::uint32_t mode) = 0;

    // Makes one best-effort recursive removal attempt.
    virtual void remove_recursively(std::string_view path) = 0;

    // Reports whether a path remains after a removal attempt.
    virtual bool path_exists(std::string_view path) = 0;

    // Renames one path without exposing platform-specific result types.
    virtual bool rename_path(std::string_view source,
                             std::string_view destination) = 0;

    // Sends one response to the destination selected by the command adapter.
    virtual void send(core::Frame frame) = 0;
};

// Executes bounded path-oriented filesystem mutations and fixed replies.
class FilesystemCommands {
public:
    // Creates a primary directory and then its mapped cache directories.
    static void make_directory(core::BytesView argument,
                               FilesystemCommandPort& port);

    // Recursively removes a path and then its mapped cache entries.
    static void remove(core::BytesView argument, FilesystemCommandPort& port);

    // Renames a primary path and then equivalent mapped cache entries.
    static void move(core::BytesView argument, FilesystemCommandPort& port);

    // Sends the fixed controller file-type compatibility response.
    static void file_type(FilesystemCommandPort& port);
};

}  // namespace firmware::application
