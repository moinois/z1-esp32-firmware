/** @file @brief Declares configuration restore/default copy policy behind bytewise file I/O. */
#pragma once

#include "firmware/core/frame.hpp"

#include <cstdint>
#include <string_view>

namespace firmware::application {

/// Distinguishes one source byte from EOF and a source read failure.
enum class ByteReadStatus {
    byte,
    end_of_file,
    failure,
};

/// Holds one bytewise source-read outcome without platform error types.
struct ByteRead {
    ByteReadStatus status;
    std::uint8_t value;
};

/// Isolates configuration copy policy from target file and response APIs.
class ConfigurationFilePort {
public:
    /// Enables safe destruction through a substituted port implementation.
    virtual ~ConfigurationFilePort() = default;

    /// Returns the active configuration path owned by the storage adapter.
    virtual std::string_view active_configuration_path() const = 0;

    /// Returns the saved default path owned by the storage adapter.
    virtual std::string_view default_configuration_path() const = 0;

    /// Reports whether the selected source file exists.
    virtual bool file_exists(std::string_view path) = 0;

    /// Opens the selected source for bytewise reading.
    virtual bool open_source(std::string_view path) = 0;

    /// Truncates or creates the selected copy destination.
    virtual bool open_truncated_destination(std::string_view path) = 0;

    /// Reads one byte, EOF, or failure from the open source.
    virtual ByteRead read_byte() = 0;

    /// Writes exactly one byte to the open destination.
    virtual bool write_byte(std::uint8_t value) = 0;

    /// Closes the source after an opened copy attempt.
    virtual void close_source() = 0;

    /// Closes the destination and reports buffered-write failure.
    virtual bool close_destination() = 0;

    /// Sends one response to the destination selected by the command adapter.
    virtual void send(core::Frame frame) = 0;
};

/// Executes fixed-path restore and saved-default configuration copies.
class ConfigurationFiles {
public:
    /// Copies the saved default over the active configuration.
    static void restore(ConfigurationFilePort& port);

    /// Copies the active configuration over the saved default.
    static void save_default(ConfigurationFilePort& port);
};

}  // namespace firmware::application
