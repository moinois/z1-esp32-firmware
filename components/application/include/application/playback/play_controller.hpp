/** @file @brief Defines streamed-play controller lifecycle handling around a shared play session. */
#pragma once

#include "application/playback/play_line_reader.hpp"
#include "application/playback/play_session.hpp"

#include <cstdint>
#include <optional>

namespace firmware::application {

/// Extends play preparation I/O with controller output and lifecycle observers.
class PlayControllerPort : public PlayPreparationPort, public PlayLineSource {
public:
    /// Submits one complete controller response and reports queue acceptance.
    virtual bool send(core::Frame frame) = 0;

    /// Notifies recording and runtime observers of play state.
    virtual void play_state_changed(bool running) = 0;

    /// Releases physical play ownership after terminal cleanup.
    virtual void release_play_ownership() = 0;

    /// Rewinds the prepared file to its first byte.
    virtual bool rewind_file() = 0;

    /// Returns monotonic time for progress-report pacing during synchronous scans.
    virtual std::uint64_t now_milliseconds() const = 0;
};

/// Processes controller start and terminal packets for streamed play.
class PlayController {
public:
    /// Uses the supplied shared session for host preparation and controller state.
    explicit PlayController(PlaySession& session);

    /// Processes one accepted `0xF` family frame.
    void handle(const core::Frame& frame, std::uint64_t now_milliseconds,
                PlayControllerPort& port);

private:
    void handle_start(core::BytesView payload, std::uint64_t now_milliseconds,
                      PlayControllerPort& port);
    void handle_data(core::BytesView payload, std::uint64_t now_milliseconds,
                     PlayControllerPort& port);
    void handle_goto(core::BytesView payload, std::uint64_t now_milliseconds,
                     PlayControllerPort& port);
    void handle_terminal(PlayControllerPort& port);
    bool seek_line(std::uint32_t target, PlayControllerPort& port);
    void send_progress(PlayControllerPort& port) const;
    void reset_read_state();

    PlaySession& session_;
    std::uint32_t session_generation_ = 0U;
    std::uint32_t current_line_ = 0U;
    std::uint64_t transmitted_bytes_ = 0U;
    std::optional<std::uint32_t> retained_index_;
    core::ByteVector retained_payload_;
};

}  // namespace firmware::application
