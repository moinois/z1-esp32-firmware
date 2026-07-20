// Defines streamed-play controller lifecycle handling around a shared play session.
#pragma once

#include "firmware/application/play_session.hpp"

#include <cstdint>

namespace firmware::application {

// Extends play preparation I/O with controller output and lifecycle observers.
class PlayControllerPort : public PlayPreparationPort {
public:
    // Submits one complete controller response and reports queue acceptance.
    virtual bool send(core::Frame frame) = 0;

    // Notifies recording and runtime observers of play state.
    virtual void play_state_changed(bool running) = 0;

    // Releases physical play ownership after terminal cleanup.
    virtual void release_play_ownership() = 0;
};

// Processes controller start and terminal packets for streamed play.
class PlayController {
public:
    // Uses the supplied shared session for host preparation and controller state.
    explicit PlayController(PlaySession& session);

    // Processes one accepted `0xF` family frame.
    void handle(const core::Frame& frame, std::uint64_t now_milliseconds,
                PlayControllerPort& port);

private:
    void handle_start(core::BytesView payload, std::uint64_t now_milliseconds,
                      PlayControllerPort& port);
    void handle_terminal(PlayControllerPort& port);

    PlaySession& session_;
};

}  // namespace firmware::application
