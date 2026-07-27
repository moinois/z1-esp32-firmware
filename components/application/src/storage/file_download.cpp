// Implements host download protocol sequencing, retries, errors, and timeout.
#include "firmware/application/file_download.hpp"

#include "firmware/core/protocol_constants.hpp"
#include "firmware/core/file_transfer_limits.hpp"
#include "firmware/core/sd_user_path.hpp"

#include <algorithm>
#include <utility>

namespace firmware::application {
namespace {

constexpr std::size_t block_size = core::file_transfer_limits::data_block_size;
constexpr std::size_t response_workspace_size =
    block_size + core::protocol::big_endian_u32_size;
constexpr std::size_t maximum_cache_read_size = 63U;
constexpr std::size_t maximum_error_path_size = 240U;
constexpr std::uint8_t maximum_unexpected_packets = 51U;
constexpr std::string_view default_md5 = "82df799dde08f3d86839e24cb97e74d4";
constexpr std::string_view timeout_error = "Error: Machine received cmd timeout!";
constexpr std::string_view allocation_error =
    "Error: download_command Memory allocation failed!";
constexpr std::string_view excessive_error =
    "Error: Machine received too many wrong command!";

// Reports whether a resolved path has the exact special-case basename.
bool is_config_file(std::string_view path) {
    const std::size_t separator = path.find_last_of('/');
    const std::string_view basename =
        separator == std::string_view::npos ? path : path.substr(separator + 1U);
    return basename == "config.txt";
}

// Encodes one unsigned 32-bit big-endian protocol value.
core::ByteVector encode_u32(std::uint32_t value) {
    return {
        static_cast<std::uint8_t>(value >> 24U),
        static_cast<std::uint8_t>(value >> 16U),
        static_cast<std::uint8_t>(value >> 8U),
        static_cast<std::uint8_t>(value),
    };
}

// Decodes one available unsigned 32-bit big-endian protocol value.
std::uint32_t decode_u32(core::BytesView bytes) {
    return (static_cast<std::uint32_t>(bytes[0]) << 24U) |
           (static_cast<std::uint32_t>(bytes[1]) << 16U) |
           (static_cast<std::uint32_t>(bytes[2]) << 8U) |
           static_cast<std::uint32_t>(bytes[3]);
}

}  // namespace

bool FileDownload::start(const HostIdentity& owner, std::string path,
                         std::uint64_t now_milliseconds, FileDownloadPort& port) {
    owner_ = owner;
    resolved_path_ = std::move(path);
    path_ = core::logical_sd_path(resolved_path_);
    const core::FileCachePaths cache_paths = core::map_file_cache_paths(resolved_path_);
    port.prepare_cache_paths(cache_paths);

    if (is_config_file(resolved_path_)) {
        const auto calculated = port.calculate_md5(resolved_path_);
        if (!calculated.has_value() ||
            calculated->size() != core::file_transfer_limits::md5_text_size) {
            send_path_error("Error: failed to get MD5 for [", port);
            return false;
        }
        md5_ = *calculated;
    } else {
        md5_ = std::string(default_md5);
        if (cache_paths.md5_path.has_value()) {
            const auto cache = port.read_cache(*cache_paths.md5_path, maximum_cache_read_size);
            if (cache.has_value()) {
                const auto extracted = core::extract_cached_md5(*cache);
                if (extracted.has_value()) {
                    md5_ = *extracted;
                }
            }
        }
    }

    std::string selected_path = resolved_path_;
    if (cache_paths.compressed_path.has_value() &&
        port.file_exists(*cache_paths.compressed_path)) {
        selected_path = *cache_paths.compressed_path;
    }
    const auto opened_size = port.open_file(selected_path);
    if (!opened_size.has_value()) {
        send_path_error("Error: failed to open file [", port);
        return false;
    }

    file_size_ = *opened_size;
    last_activity_milliseconds_ = now_milliseconds;
    unexpected_count_ = 0U;
    active_ = true;
    send_md5(port);
    return true;
}

void FileDownload::handle(const core::Frame& frame,
                          std::uint64_t now_milliseconds,
                          FileDownloadPort& port) {
    if (!active_) {
        return;
    }
    last_activity_milliseconds_ = now_milliseconds;
    switch (frame.type) {
        case core::protocol::file_md5:
            send_md5(port);
            break;
        case core::protocol::file_geometry:
            send_geometry(port);
            break;
        case core::protocol::file_data:
            if (frame.payload.size() < core::protocol::big_endian_u32_size) {
                record_unexpected(port);
            } else {
                send_data(decode_u32(frame.payload), port);
            }
            break;
        case core::protocol::file_complete:
            port.send(owner_, {core::protocol::file_complete, {'o', 'k', '\r', '\n'}});
            retained_response_.payload.assign(
                {'I', 'n', 'f', 'o', ':', ' ', 'd', 'o', 'w', 'n', 'l', 'o', 'a', 'd', ' ',
                 's', 'u', 'c', 'c', 'e', 's', 's', ':', ' '});
            retained_response_.payload.insert(retained_response_.payload.end(), path_.begin(), path_.end());
            retained_response_.payload.push_back('.');
            port.send(owner_, {core::protocol::console_message, retained_response_.payload});
            finish(port);
            break;
        case core::protocol::file_cancel:
            port.send(owner_, {core::protocol::file_cancel,
                               {'I', 'n', 'f', 'o', ':', ' ', 'c', 'a', 'n', 'c', 'e', 'l',
                                        'e', 'd', ' ', 'b', 'y', ' ', 'r', 'e', 'm', 'o', 't', 'e', '!'}});
            finish(port);
            break;
        case core::protocol::file_retry:
            retry_last(port);
            break;
        default:
            record_unexpected(port);
            break;
    }
}

void FileDownload::poll(std::uint64_t now_milliseconds, FileDownloadPort& port) {
    if (active_ && now_milliseconds - last_activity_milliseconds_ >
                       core::file_transfer_limits::inactivity_timeout_milliseconds) {
        abort(timeout_error, port);
    }
}

bool FileDownload::active() const {
    return active_;
}

void FileDownload::send_md5(FileDownloadPort& port) {
    retained_response_ = {core::protocol::file_md5, {md5_.begin(), md5_.end()}};
    port.send(owner_, retained_response_);
    last_response_ = LastResponse::md5;
    unexpected_count_ = 0U;
}

void FileDownload::send_geometry(FileDownloadPort& port) {
    const std::uint64_t count = (file_size_ + block_size - 1U) / block_size;
    core::ByteVector payload = encode_u32(static_cast<std::uint32_t>(count));
    payload.push_back(static_cast<std::uint8_t>(block_size >> 8U));
    payload.push_back(static_cast<std::uint8_t>(block_size));
    retained_response_ = {core::protocol::file_geometry, std::move(payload)};
    port.send(owner_, retained_response_);
    last_response_ = LastResponse::geometry;
    unexpected_count_ = 0U;
}

void FileDownload::send_data(std::uint32_t sequence, FileDownloadPort& port) {
    if (sequence == 0U || !port.allocate_response_workspace(response_workspace_size)) {
        abort(sequence == 0U ? timeout_error : allocation_error, port);
        return;
    }
    const std::uint64_t offset = static_cast<std::uint64_t>(sequence - 1U) * block_size;
    if (offset >= file_size_) {
        abort(timeout_error, port);
        return;
    }
    auto data = port.read_file(offset, block_size);
    if (!data.has_value() || data->empty()) {
        abort(timeout_error, port);
        return;
    }
    if (data->size() > block_size) {
        data->resize(block_size);
    }
    core::ByteVector payload = encode_u32(sequence);
    payload.insert(payload.end(), data->begin(), data->end());
    retained_response_ = {core::protocol::file_data, std::move(payload)};
    port.send(owner_, retained_response_);
    last_data_sequence_ = sequence;
    last_response_ = LastResponse::data;
    unexpected_count_ = 0U;
}

void FileDownload::retry_last(FileDownloadPort& port) {
    if (last_response_ == LastResponse::data) {
        send_data(last_data_sequence_, port);
        return;
    }
    if (last_response_ == LastResponse::md5 || last_response_ == LastResponse::geometry) {
        port.send(owner_, retained_response_);
        unexpected_count_ = 0U;
        return;
    }
    record_unexpected(port);
}

void FileDownload::record_unexpected(FileDownloadPort& port) {
    ++unexpected_count_;
    if (unexpected_count_ >= maximum_unexpected_packets) {
        abort(excessive_error, port);
    }
}

void FileDownload::abort(std::string_view message, FileDownloadPort& port) {
    port.send(owner_, {core::protocol::file_cancel, {message.begin(), message.end()}});
    finish(port);
}

void FileDownload::finish(FileDownloadPort& port) {
    if (active_) {
        port.close_file();
    }
    active_ = false;
    port.release_ownership();
}

void FileDownload::send_path_error(std::string_view prefix, FileDownloadPort& port) {
    core::ByteVector payload(prefix.begin(), prefix.end());
    const std::size_t inserted_size = std::min(path_.size(), maximum_error_path_size);
    payload.insert(payload.end(), path_.begin(),
                   path_.begin() + static_cast<std::ptrdiff_t>(inserted_size));
    payload.insert(payload.end(), {']', '!'});
    port.send(owner_, {core::protocol::file_cancel, std::move(payload)});
    active_ = false;
    port.release_ownership();
}

}  // namespace firmware::application
