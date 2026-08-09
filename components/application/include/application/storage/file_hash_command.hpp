/** @file @brief Declares MD5 command policy behind replaceable path and hashing operations. */
#pragma once

#include "core/protocol/bytes.hpp"
#include "core/protocol/frame.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace firmware::application {

/// Distinguishes silent resolution failure from observable path kinds.
enum class FileHashPathState {
    resolution_failure,
    missing,
    not_regular,
    regular_file,
};

/// Isolates MD5 command policy from filesystem metadata, hashing, and transport.
class FileHashPort {
public:
    /// Enables safe destruction through a substituted port implementation.
    virtual ~FileHashPort() = default;

    /// Resolves and classifies a normalized path for the hash operation.
    virtual FileHashPathState inspect_path(std::string_view path) = 0;

    /// Hashes all file bytes using reads no larger than the requested block.
    virtual std::optional<std::string> calculate_md5(
        std::string_view path, std::size_t block_size) = 0;

    /// Sends one response to the destination selected by the command adapter.
    virtual void send(core::Frame frame) = 0;
};

/// Executes one validated `md5sum` request and its exact result mapping.
class FileHashCommand {
public:
    /// Parses, classifies, hashes, and responds to one command argument.
    static void execute(core::BytesView argument, FileHashPort& port);
};

}  // namespace firmware::application
