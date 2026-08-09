/** @file @brief Implements streamed-play path preparation, identity, and status behavior. */
#include "application/playback/play_session.hpp"

#include "core/protocol/crc.hpp"
#include "core/filesystem/file_transfer_limits.hpp"
#include "core/protocol/protocol_constants.hpp"
#include "core/filesystem/sd_user_path.hpp"
#include "core/protocol/text.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <utility>

namespace firmware::application {
namespace {

constexpr std::uint64_t error_interval_milliseconds = 1000U;
constexpr std::size_t play_prefix_size = 5U;
constexpr std::string_view open_error = "Error:open file failed[P0]";

// Selects and normalizes the path text according to the unusual play-prefix rule.
std::string resolve_play_path(core::BytesView payload) {
    std::string decoded = core::decode_escaped(payload);
    if (!decoded.empty() && decoded.back() == '\n') {
        decoded.pop_back();
    }

    std::string selected = decoded;
    if (decoded.size() > play_prefix_size &&
        decoded.compare(0U, play_prefix_size, "play ") == 0) {
        selected = decoded.substr(play_prefix_size);
    }
    const auto first = std::find_if(selected.begin(), selected.end(), [](char value) {
        return value != ' ';
    });
    selected.erase(selected.begin(), first);
    return core::resolve_sd_user_path(selected);
}

// Reports whether a cached checksum contains exactly 32 hexadecimal characters.
bool valid_md5(std::string_view value) {
    return value.size() == core::file_transfer_limits::md5_text_size &&
           std::all_of(value.begin(), value.end(), [](unsigned char character) {
               return std::isxdigit(character) != 0;
           });
}

}  // namespace

bool PlaySession::prepare(core::BytesView payload, std::uint64_t now_milliseconds,
                          PlayPreparationPort& port) {
    port.close_file();
    clear_prepared_state();
    ++generation_;

    const std::string resolved = resolve_play_path(payload);
    if (resolved.size() > core::file_transfer_limits::maximum_path_size) {
        report_error(open_error, now_milliseconds, port);
        return false;
    }
    const auto opened_size = port.open_file(resolved);
    if (!opened_size.has_value() ||
        *opened_size > std::numeric_limits<std::uint32_t>::max()) {
        if (opened_size.has_value()) {
            port.close_file();
        }
        report_error(open_error, now_milliseconds, port);
        return false;
    }

    path_ = core::logical_sd_path(resolved);
    file_size_ = static_cast<std::uint32_t>(*opened_size);
    path_identifier_ = core::crc16_ccitt(std::string_view(path_));
    const auto cached = port.cached_md5(resolved);
    if (cached.has_value() && valid_md5(*cached)) {
        md5_ = *cached;
    }
    file_open_ = true;
    return true;
}

core::Frame PlaySession::status_reply() const {
    if (!running_ || !file_open_) {
        return {core::protocol::play_status, {'|'}};
    }
    core::ByteVector payload(path_.begin(), path_.end());
    payload.push_back('|');
    payload.insert(payload.end(), md5_.begin(), md5_.end());
    return {core::protocol::play_status, std::move(payload)};
}

void PlaySession::mark_running() {
    running_ = true;
}

void PlaySession::terminate(PlayPreparationPort& port) {
    port.close_file();
    running_ = false;
    clear_prepared_state();
}

bool PlaySession::running() const {
    return running_;
}

bool PlaySession::file_open() const {
    return file_open_;
}

std::uint16_t PlaySession::path_identifier() const {
    return path_identifier_;
}

std::uint32_t PlaySession::file_size() const {
    return file_size_;
}

std::string_view PlaySession::path() const {
    return path_;
}

std::uint32_t PlaySession::generation() const {
    return generation_;
}

void PlaySession::report_error(std::string_view message,
                               std::uint64_t now_milliseconds,
                               PlayPreparationPort& port) {
    if (last_error_milliseconds_.has_value() &&
        now_milliseconds - *last_error_milliseconds_ < error_interval_milliseconds) {
        return;
    }
    last_error_milliseconds_ = now_milliseconds;
    port.broadcast({core::protocol::console_message, {message.begin(), message.end()}});
}

void PlaySession::clear_prepared_state() {
    file_open_ = false;
    path_.clear();
    md5_.clear();
    path_identifier_ = 0U;
    file_size_ = 0U;
    current_line_ = 0U;
    transmitted_bytes_ = 0U;
    retained_data_.clear();
}

}  // namespace firmware::application
