/** @file @brief Implements streamed-play controller acceptance and terminal cleanup. */
#include "application/playback/play_controller.hpp"

#include "core/protocol/protocol_constants.hpp"

#include <string_view>
#include <utility>

namespace firmware::application {
namespace {

constexpr std::string_view start_error =
    "Error:start check failed.file does not exist or CRC wrong [P1]";
constexpr std::string_view data_format_error =
    "Error:PTYPE_PLAY_DATA command data format error [P2]";
constexpr std::string_view goto_check_error =
    "Error:goto check failed.file does not exist or CRC wrong [P3]";
constexpr std::string_view goto_format_error =
    "Error:PTYPE_PLAY_DATA goto cmd format error [P3]";
constexpr std::size_t maximum_data_size = 512U;
constexpr std::size_t minimum_remaining_data = 74U;
constexpr std::size_t identifier_size = core::protocol::big_endian_u16_size;
constexpr std::size_t request_index_offset = identifier_size;
constexpr std::size_t data_request_size = identifier_size + core::protocol::big_endian_u32_size;
constexpr std::size_t request_with_line_limit_size = data_request_size + identifier_size;
constexpr std::uint16_t default_maximum_lines = 255U;
constexpr std::uint64_t progress_interval_milliseconds = 100U;
constexpr std::uint8_t play_start_response = core::protocol::family_packet(
    core::protocol::play_family, core::protocol::transfer_geometry);
constexpr std::uint8_t play_data_response = core::protocol::family_packet(
    core::protocol::play_family, core::protocol::transfer_data);
constexpr std::uint8_t play_complete_response = core::protocol::family_packet(
    core::protocol::play_family, core::protocol::transfer_complete);
constexpr std::uint8_t play_error_response = core::protocol::family_packet(
    core::protocol::play_family, core::protocol::transfer_cancel);
constexpr std::uint8_t play_progress_response = core::protocol::family_packet(
    core::protocol::play_family, core::protocol::play_progress);

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

// Encodes an accepted identifier and file size for the play-start response.
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
    const std::uint8_t operation = frame.type & core::protocol::operation_mask;
    switch (operation) {
        case core::protocol::transfer_start:
            handle_start(frame.payload, now_milliseconds, port);
            break;
        case core::protocol::transfer_complete:
        case core::protocol::transfer_cancel:
            handle_terminal(port);
            break;
        case core::protocol::transfer_geometry:
        case core::protocol::transfer_data:
            if (operation == core::protocol::transfer_data) {
                handle_data(frame.payload, now_milliseconds, port);
            }
            break;
        case core::protocol::play_goto:
            handle_goto(frame.payload, now_milliseconds, port);
            break;
        case core::protocol::play_progress:
        default:
            break;
    }
}

void PlayController::handle_goto(core::BytesView payload,
                                 std::uint64_t now_milliseconds,
                                 PlayControllerPort& port) {
    const bool identifier_present = payload.size() >= identifier_size;
    const bool identifier_matches = identifier_present &&
                                    decode_identifier(payload) == session_.path_identifier();
    if (!session_.file_open() || (identifier_present && !identifier_matches)) {
        static_cast<void>(port.send({play_complete_response, {}}));
        session_.report_error(goto_check_error, now_milliseconds, port);
        return;
    }
    if (payload.size() < data_request_size) {
        static_cast<void>(port.send({play_complete_response, {}}));
        session_.report_error(goto_format_error, now_milliseconds, port);
        return;
    }

    retained_index_.reset();
    retained_payload_.clear();
    if (!port.rewind_file()) {
        static_cast<void>(port.send({play_error_response, {}}));
        return;
    }
    current_line_ = 0U;
    transmitted_bytes_ = 0U;
    const std::uint32_t target = decode_u32(payload.data() + request_index_offset);
    std::uint64_t last_progress = port.now_milliseconds();

    for (;;) {
        const PlayLineResult line = PlayLineReader::read(port);
        if (line.status == PlayLineStatus::failure) {
            static_cast<void>(port.send({play_complete_response, {}}));
            return;
        }
        if (line.status == PlayLineStatus::end_of_file) {
            send_progress(port);
            return;
        }

        ++current_line_;
        transmitted_bytes_ += line.data.size();
        const bool reached_target = !line.data.empty() &&
                                    (target == 0U || current_line_ >= target);
        const std::uint64_t now = port.now_milliseconds();
        const bool interval_elapsed = now - last_progress > progress_interval_milliseconds;
        if (interval_elapsed || reached_target) {
            send_progress(port);
            last_progress = now;
        }
        if (line.reached_eof) {
            send_progress(port);
            return;
        }
        if (reached_target) {
            return;
        }
    }
}

void PlayController::handle_data(core::BytesView payload,
                                 std::uint64_t now_milliseconds,
                                 PlayControllerPort& port) {
    if (!session_.file_open()) {
        static_cast<void>(port.send({play_complete_response, {}}));
        return;
    }
    if (payload.size() >= identifier_size &&
        decode_identifier(payload) != session_.path_identifier()) {
        static_cast<void>(port.send({play_complete_response, {}}));
        return;
    }
    if (payload.size() < data_request_size) {
        static_cast<void>(port.send({play_complete_response, {}}));
        session_.report_error(data_format_error, now_milliseconds, port);
        return;
    }

    const std::uint32_t requested_index = decode_u32(payload.data() + request_index_offset);
    std::uint16_t maximum_lines = default_maximum_lines;
    if (payload.size() >= request_with_line_limit_size) {
        maximum_lines = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(payload[data_request_size]) << 8U) |
            payload[data_request_size + 1U]);
        if (maximum_lines == 0U) {
            maximum_lines = default_maximum_lines;
        }
    }
    if (retained_index_ == requested_index && !retained_payload_.empty()) {
        static_cast<void>(port.send({play_data_response, retained_payload_}));
        return;
    }
    if (requested_index != current_line_ && !seek_line(requested_index, port)) {
        static_cast<void>(port.send({play_complete_response, {}}));
        return;
    }

    core::ByteVector data;
    bool reached_eof = false;
    for (std::uint16_t line_count = 0U; line_count < maximum_lines; ++line_count) {
        const PlayLineResult line = PlayLineReader::read(port);
        if (line.status == PlayLineStatus::failure) {
            static_cast<void>(port.send({play_complete_response, {}}));
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
        core::ByteVector response(payload.begin(), payload.begin() + data_request_size);
        response.insert(response.end(), data.begin(), data.end());
        static_cast<void>(port.send({play_data_response, response}));
        retained_index_ = requested_index;
        retained_payload_ = std::move(response);
    }
    if (reached_eof) {
        static_cast<void>(port.send({play_complete_response, {}}));
        handle_terminal(port);
    }
}

void PlayController::handle_start(core::BytesView payload,
                                  std::uint64_t now_milliseconds,
                                  PlayControllerPort& port) {
    const bool valid = payload.size() >= identifier_size && session_.file_open() &&
                       decode_identifier(payload) == session_.path_identifier();
    if (!valid) {
        static_cast<void>(port.send({play_error_response, {}}));
        session_.report_error(start_error, now_milliseconds, port);
        return;
    }

    const core::ByteVector response =
        encode_start_response(session_.path_identifier(), session_.file_size());
    // PLAY-023 makes controller-output admission observational only: the
    // enclosing operation advances exactly as if the response was offered.
    static_cast<void>(port.send({play_start_response, response}));
    session_.mark_running();
    port.play_state_changed(true);
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

void PlayController::send_progress(PlayControllerPort& port) const {
    const std::uint32_t bytes = static_cast<std::uint32_t>(transmitted_bytes_);
    core::ByteVector payload{
        static_cast<std::uint8_t>(session_.path_identifier() >> 8U),
        static_cast<std::uint8_t>(session_.path_identifier()),
        static_cast<std::uint8_t>(current_line_ >> 24U),
        static_cast<std::uint8_t>(current_line_ >> 16U),
        static_cast<std::uint8_t>(current_line_ >> 8U),
        static_cast<std::uint8_t>(current_line_),
        static_cast<std::uint8_t>(bytes >> 24U),
        static_cast<std::uint8_t>(bytes >> 16U),
        static_cast<std::uint8_t>(bytes >> 8U),
        static_cast<std::uint8_t>(bytes),
    };
    static_cast<void>(port.send({play_progress_response, std::move(payload)}));
}

void PlayController::reset_read_state() {
    current_line_ = 0U;
    transmitted_bytes_ = 0U;
    retained_index_.reset();
    retained_payload_.clear();
}

}  // namespace firmware::application
