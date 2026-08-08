/** @file @brief Declares deterministic directory-list policy behind a replaceable filesystem port. */
#pragma once

#include "firmware/core/bytes.hpp"
#include "firmware/core/frame.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace firmware::application {

/// Holds a filesystem modification time already converted to UTC.
struct UtcFileTime {
    std::uint16_t year;
    std::uint8_t month;
    std::uint8_t day;
    std::uint8_t hour;
    std::uint8_t minute;
    std::uint8_t second;
};

/// Describes one enumerated entry without exposing platform stat structures.
struct DirectoryEntry {
    std::string name;
    bool directory;
    std::uint64_t size;
    UtcFileTime modified;
    bool metadata_available;
};

/// Isolates listing policy from directory enumeration and response transport.
class DirectoryListPort {
public:
    /// Enables safe destruction through a substituted port implementation.
    virtual ~DirectoryListPort() = default;

    /// Enumerates a resolved directory in filesystem order or reports failure.
    virtual std::optional<std::vector<DirectoryEntry>> list_directory(
        std::string_view path) = 0;

    /// Sends one response to the command's destination selected by its adapter.
    virtual void send(core::Frame frame) = 0;
};

/// Formats and chunks one complete `ls` operation.
class DirectoryListing {
public:
    /// Executes an argument and always emits the terminal completion response.
    static void execute(core::BytesView argument, DirectoryListPort& port);
};

}  // namespace firmware::application
