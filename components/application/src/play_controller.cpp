// Implements streamed-play controller acceptance and terminal cleanup.
#include "firmware/application/play_controller.hpp"

#include <string_view>
#include <utility>

namespace firmware::application {
namespace {

constexpr std::string_view start_error =
    "Error:start check failed.file does not exist or CRC wrong [P1]";
constexpr std::string_view data_format_error =
    "Error:PTYPE_PLAY_DATA command data format error [P2]";
constexpr std::size_t maximum_data_size = 512U;
constexpr std::size_t minimum_remaining_data = 74U;

// Decodes an unsigned 16-bit big-endian identifier from two available bytes.
std::uint16_t decode_identifier(core::BytesView payload) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(payload[0]) << 8U) |
                                      payload[1]);
}

// Decodes an unsigned 32-bit big-endian value from four available bytes.
std::uint32_t decode_u32(const std::uint8_t* bytes) {
    return (static_cast<std::uint32_t>(bytes[0]) << 24U) |
           (static_cast<std::uint32_t>(bytes[1]) << 16U) |
           (static_cast<std::uint32_t>(bytes[2]) << 8U) |
           static_cast<std::uint32_t>(bytes[3]);
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
    if (session_generation_ != session_.generation()) {
        session_generation_ = session_.generation();
        reset_read_state();
    }
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
            if ((frame.type & 0x0FU) == 3U) {
                handle_data(frame.payload, now_milliseconds, port);
            }
            break;
        case 6U:
        case 7U:
        default:
            break;
    }
}

void PlayController::handle_data(core::BytesView payload,
                                 std::uint64_t now_milliseconds,
                                 PlayControllerPort& port) {
    if (!session_.file_open()) {
        static_cast<void>(port.send({0xF4U, {}}));
        return;
    }
    if (payload.size() >= 2U && decode_identifier(payload) != session_.path_identifier()) {
        static_cast<void>(port.send({0xF4U, {}}));
        return;
    }
    if (payload.size() < 6U) {
        static_cast<void>(port.send({0xF4U, {}}));
        session_.report_error(data_format_error, now_milliseconds, port);
        return;
    }

    const std::uint32_t requested_index = decode_u32(payload.data() + 2U);
    std::uint16_t maximum_lines = 255U;
    if (payload.size() >= 8U) {
        maximum_lines = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(payload[6]) << 8U) | payload[7]);
        if (maximum_lines == 0U) {
            maximum_lines = 255U;
        }
    }
    if (retained_index_ == requested_index && !retained_payload_.empty()) {
        static_cast<void>(port.send({0xF3U, retained_payload_}));
        return;
    }
    if (requested_index != current_line_ && !seek_line(requested_index, port)) {
        static_cast<void>(port.send({0xF4U, {}}));
        return;
    }

    core::ByteVector data;
    bool reached_eof = false;
    for (std::uint16_t line_count = 0U; line_count < maximum_lines; ++line_count) {
        const PlayLineResult line = PlayLineReader::read(port);
        if (line.status == PlayLineStatus::failure) {
            static_cast<void>(port.send({0xF4U, {}}));
            return;
        }
        if (line.status == PlayLineStatus::end_of_file) {
            reached_eof = true;
            break;
        }
        ++current_line_;
        transmitted_bytes_ += line.data.size();
        if (line.data.empty()) {
            return;
        }
        data.insert(data.end(), line.data.begin(), line.data.end());
        reached_eof = line.reached_eof;
        if (reached_eof || maximum_data_size - data.size() < minimum_remaining_data) {
            break;
        }
    }

    if (!data.empty()) {
        core::ByteVector response(payload.begin(), payload.begin() + 6);
        response.insert(response.end(), data.begin(), data.end());
        static_cast<void>(port.send({0xF3U, response}));
        retained_index_ = requested_index;
        retained_payload_ = std::move(response);
    }
    if (reached_eof) {
        static_cast<void>(port.send({0xF4U, {}}));
        handle_terminal(port);
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
    reset_read_state();
    port.play_state_changed(false);
    port.release_play_ownership();
}

bool PlayController::seek_line(std::uint32_t target, PlayControllerPort& port) {
    if (!port.rewind_file()) {
        return false;
    }
    current_line_ = 0U;
    transmitted_bytes_ = 0U;
    while (current_line_ < target) {
        const PlayLineResult line = PlayLineReader::read(port);
        if (line.status != PlayLineStatus::line) {
            return false;
        }
        ++current_line_;
        transmitted_bytes_ += line.data.size();
        if (line.reached_eof && current_line_ < target) {
            return false;
        }
    }
    return true;
}

void PlayController::reset_read_state() {
    current_line_ = 0U;
    transmitted_bytes_ = 0U;
    retained_index_.reset();
    retained_payload_.clear();
}

}  // namespace firmware::application
