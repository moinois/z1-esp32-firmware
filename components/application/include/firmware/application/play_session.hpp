/** @file @brief Defines streamed-play preparation and shared session state behind replaceable I/O. */
#pragma once

#include "firmware/core/frame.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace firmware::application {

/// Isolates play preparation from filesystem, cache, and broadcast adapters.
class PlayPreparationPort {
public:
    /// Enables safe destruction through a substituted port implementation.
    virtual ~PlayPreparationPort() = default;

    /// Closes any previously prepared file handle.
    virtual void close_file() = 0;

    /// Opens the resolved path and returns its byte size or failure.
    virtual std::optional<std::uint64_t> open_file(std::string_view path) = 0;

    /// Returns a validated cache candidate for the resolved path when available.
    virtual std::optional<std::string> cached_md5(std::string_view path) = 0;

    /// Broadcasts one console frame to active host destinations.
    virtual void broadcast(core::Frame frame) = 0;
};

/// Owns prepared streamed-play identity and status across host and controller flows.
class PlaySession {
public:
    /// Replaces the prepared file using one accepted host play command.
    bool prepare(core::BytesView payload, std::uint64_t now_milliseconds,
                 PlayPreparationPort& port);

    /// Creates the local `0xB7` status response for the requesting host.
    core::Frame status_reply() const;

    /// Marks controller acceptance without resetting current playback position.
    void mark_running();

    /// Clears running and prepared state after a terminal controller operation.
    void terminate(PlayPreparationPort& port);

    /// Reports whether the controller-facing observers consider play active.
    bool running() const;

    /// Reports whether a prepared file is currently open.
    bool file_open() const;

    /// Returns the common CRC identifier of the currently prepared path.
    std::uint16_t path_identifier() const;

    /// Returns the prepared file size constrained to the protocol width.
    std::uint32_t file_size() const;

    /// Returns the normalized path used for opening and status.
    std::string_view path() const;

    /// Changes whenever preparation replaces and resets controller read state.
    std::uint32_t generation() const;

    /// Broadcasts a play error subject to the shared one-second rate limit.
    void report_error(std::string_view message, std::uint64_t now_milliseconds,
                      PlayPreparationPort& port);

private:
    void clear_prepared_state();

    bool file_open_ = false;
    bool running_ = false;
    std::string path_;
    std::string md5_;
    std::uint16_t path_identifier_ = 0U;
    std::uint32_t file_size_ = 0U;
    std::uint32_t current_line_ = 0U;
    std::uint64_t transmitted_bytes_ = 0U;
    core::ByteVector retained_data_;
    std::optional<std::uint64_t> last_error_milliseconds_;
    std::uint32_t generation_ = 0U;
};

}  // namespace firmware::application
