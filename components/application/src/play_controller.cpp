// Implements streamed-play controller acceptance and terminal cleanup.
#include "firmware/application/play_controller.hpp"

#include <string_view>

namespace firmware::application {
namespace {

constexpr std::string_view start_error =
    "Error:start check failed.file does not exist or CRC wrong [P1]";

// Decodes an unsigned 16-bit big-endian identifier from two available bytes.
std::uint16_t decode_identifier(core::BytesView payload) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(payload[0]) << 8U) |
                                      payload[1]);
}

// Encodes an accepted identifier and file size for the `0xF2` response.
core::ByteVector encode_start_response(std::uint16_t identifier, std::uint32_t file_size) {
    return {
        static_cast<std::uint8_t>(identifier >> 8U),
        static_cast<std::uint8_t>(identifier),
        static_cast<std::uint8_t>(file_size >> 24U),
        static_cast<std::uint8_t>(file_size >> 16U),
        static_cast<std::uint8_t>(file_size >> 8U),
        static_cast<std::uint8_t>(file_size),
    };
}

}  // namespace

PlayController::PlayController(PlaySession& session)
    : session_(session) {}

void PlayController::handle(const core::Frame& frame, std::uint64_t now_milliseconds,
                            PlayControllerPort& port) {
    switch (frame.type & 0x0FU) {
        case 1U:
            handle_start(frame.payload, now_milliseconds, port);
            break;
        case 4U:
        case 5U:
            handle_terminal(port);
            break;
        case 2U:
        case 3U:
        case 6U:
        case 7U:
        default:
            break;
    }
}

void PlayController::handle_start(core::BytesView payload,
                                  std::uint64_t now_milliseconds,
                                  PlayControllerPort& port) {
    const bool valid = payload.size() >= 2U && session_.file_open() &&
                       decode_identifier(payload) == session_.path_identifier();
    if (!valid) {
        static_cast<void>(port.send({0xF5U, {}}));
        session_.report_error(start_error, now_milliseconds, port);
        return;
    }

    const core::ByteVector response =
        encode_start_response(session_.path_identifier(), session_.file_size());
    if (port.send({0xF2U, response})) {
        session_.mark_running();
        port.play_state_changed(true);
    }
}

void PlayController::handle_terminal(PlayControllerPort& port) {
    session_.terminate(port);
    port.play_state_changed(false);
    port.release_play_ownership();
}

}  // namespace firmware::application
