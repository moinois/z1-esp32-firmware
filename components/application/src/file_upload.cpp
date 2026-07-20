// Implements host upload sequencing, file lifecycle, retries, and timeout.
#include "firmware/application/file_upload.hpp"

#include "firmware/core/protocol_constants.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace firmware::application {
namespace {

constexpr std::size_t maximum_data_size = 8192U;
constexpr std::size_t md5_text_size = 32U;
constexpr std::size_t sequence_size = core::protocol::big_endian_u32_size;
constexpr std::uint64_t timed_retry_interval_milliseconds = 5010U;
constexpr std::uint64_t inactivity_timeout_milliseconds = 9000U;
constexpr std::uint8_t packets_per_retry_cycle = 51U;
constexpr std::uint8_t maximum_retry_cycles = 51U;
constexpr std::uint32_t parent_directory_mode = 0777U;
constexpr std::string_view firmware_path = "/sd/firmware.bin";
constexpr std::string_view firmware_partial_path = "/sd/firmware.bin.part";
constexpr std::string_view timeout_message = "Info: Machine receive file time out!";
constexpr std::string_view excessive_retry_message =
    "Info: Machine receive file too many retry error!";

// Compares ASCII path text without case for the firmware special case.
bool ascii_case_equal(std::string_view left, std::string_view right) {
    return left.size() == right.size() &&
           std::equal(left.begin(), left.end(), right.begin(), [](unsigned char a,
                                                                  unsigned char b) {
               return std::tolower(a) == std::tolower(b);
           });
}

// Encodes one unsigned 32-bit big-endian sequence.
core::ByteVector encode_u32(std::uint32_t value) {
    return {
        static_cast<std::uint8_t>(value >> 24U),
        static_cast<std::uint8_t>(value >> 16U),
        static_cast<std::uint8_t>(value >> 8U),
        static_cast<std::uint8_t>(value),
    };
}

// Decodes one available unsigned 32-bit big-endian value.
std::uint32_t decode_u32(core::BytesView bytes) {
    return (static_cast<std::uint32_t>(bytes[0]) << 24U) |
           (static_cast<std::uint32_t>(bytes[1]) << 16U) |
           (static_cast<std::uint32_t>(bytes[2]) << 8U) |
           static_cast<std::uint32_t>(bytes[3]);
}

// Builds an upload console message containing the retained logical path.
core::ByteVector path_message(std::string_view prefix, std::string_view path,
                              std::string_view suffix) {
    core::ByteVector result(prefix.begin(), prefix.end());
    result.insert(result.end(), path.begin(), path.end());
    result.insert(result.end(), suffix.begin(), suffix.end());
    return result;
}

}  // namespace

bool FileUpload::start(const HostIdentity& owner, std::string path,
                       std::uint64_t now_milliseconds, FileUploadPort& port) {
    owner_ = owner;
    logical_path_ = std::move(path);
    const core::FileCachePaths cache_paths = core::map_file_cache_paths(logical_path_);
    port.prepare_cache_paths(cache_paths);
    if (!cache_paths.md5_path.has_value()) {
        port.send(owner_, {core::protocol::file_cancel,
                           {'E', 'r', 'r', 'o', 'r', ':', ' ', 'I', 'n', 'v', 'a', 'l',
                                    'i', 'd', ' ', 'f', 'i', 'l', 'e', 'n', 'a', 'm', 'e', '!'}});
        port.release_ownership();
        return false;
    }

    md5_path_ = *cache_paths.md5_path;
    if (!port.create_parent_directories(md5_path_, parent_directory_mode)) {
        port.send(owner_, {core::protocol::file_cancel,
                           path_message("Error: failed to open file [", logical_path_, "]!")});
        port.release_ownership();
        return false;
    }

    firmware_upload_ = ascii_case_equal(logical_path_, firmware_path);
    target_path_ = firmware_upload_ ? std::string(firmware_partial_path) : logical_path_;
    if (firmware_upload_) {
        static_cast<void>(port.remove_file(firmware_partial_path));
    }

    const bool primary_open = port.open_primary(target_path_);
    const bool md5_open = primary_open && port.open_md5(md5_path_);
    if (!primary_open || !md5_open) {
        if (primary_open) {
            port.close_files();
            static_cast<void>(port.remove_file(target_path_));
        }
        port.send(owner_, {core::protocol::file_cancel,
                           path_message("Error: failed to open file [", logical_path_, "]!")});
        port.release_ownership();
        return false;
    }

    expected_ = ExpectedPacket::md5;
    requested_sequence_ = 1U;
    announced_frame_count_ = 0U;
    reset_retry_counters();
    last_activity_milliseconds_ = now_milliseconds;
    next_timed_retry_milliseconds_ = now_milliseconds + timed_retry_interval_milliseconds;
    active_ = true;
    return true;
}

void FileUpload::handle(const core::Frame& frame,
                        std::uint64_t now_milliseconds,
                        FileUploadPort& port) {
    if (!active_) {
        return;
    }
    last_activity_milliseconds_ = now_milliseconds;
    next_timed_retry_milliseconds_ = now_milliseconds + timed_retry_interval_milliseconds;
    if (frame.type == core::protocol::file_cancel) {
        cancel(port);
        return;
    }

    const bool md5_valid = expected_ == ExpectedPacket::md5 &&
                           frame.type == core::protocol::file_md5 &&
                           frame.payload.size() >= md5_text_size;
    const bool geometry_valid = expected_ == ExpectedPacket::geometry &&
                                frame.type == core::protocol::file_geometry &&
                                frame.payload.size() >= sequence_size;
    const bool data_valid = expected_ == ExpectedPacket::data &&
                            frame.type == core::protocol::file_data &&
                            frame.payload.size() >= sequence_size &&
                            frame.payload.size() <= maximum_data_size + sequence_size &&
                            decode_u32(frame.payload) == requested_sequence_;
    if (md5_valid) {
        accept_md5(frame.payload, port);
    } else if (geometry_valid) {
        accept_geometry(frame.payload, port);
    } else if (data_valid) {
        accept_data(frame.payload, port);
    } else {
        record_unexpected(port);
    }
}

void FileUpload::poll(std::uint64_t now_milliseconds, FileUploadPort& port) {
    if (!active_) {
        return;
    }
    if (now_milliseconds - last_activity_milliseconds_ > inactivity_timeout_milliseconds) {
        abort(timeout_message, port);
        return;
    }
    if (now_milliseconds >= next_timed_retry_milliseconds_) {
        emit_timed_retry(port);
        const std::uint64_t periods =
            ((now_milliseconds - next_timed_retry_milliseconds_) /
             timed_retry_interval_milliseconds) + 1U;
        next_timed_retry_milliseconds_ += periods * timed_retry_interval_milliseconds;
    }
}

bool FileUpload::active() const {
    return active_;
}

void FileUpload::accept_md5(core::BytesView payload, FileUploadPort& port) {
    static_cast<void>(port.write_md5({payload.data(), md5_text_size}));
    port.send(owner_, {core::protocol::file_geometry, {}});
    expected_ = ExpectedPacket::geometry;
    reset_retry_counters();
}

void FileUpload::accept_geometry(core::BytesView payload, FileUploadPort& port) {
    announced_frame_count_ = decode_u32(payload);
    requested_sequence_ = 1U;
    port.send(owner_, {core::protocol::file_data, encode_u32(requested_sequence_)});
    expected_ = ExpectedPacket::data;
    reset_retry_counters();
}

void FileUpload::accept_data(core::BytesView payload, FileUploadPort& port) {
    const core::BytesView data{payload.data() + sequence_size,
                               payload.size() - sequence_size};
    if (!port.write_primary(data)) {
        constexpr std::string_view error = "Error: File Write error!retry...";
        port.send(owner_, {core::protocol::file_retry, {error.begin(), error.end()}});
        return;
    }
    reset_retry_counters();
    if (requested_sequence_ >= announced_frame_count_) {
        complete(port);
        return;
    }
    ++requested_sequence_;
    port.send(owner_, {core::protocol::file_data, encode_u32(requested_sequence_)});
}

void FileUpload::complete(FileUploadPort& port) {
    port.send(owner_, {core::protocol::file_complete, {'o', 'k', '\r', '\n'}});
    port.flush_and_close();
    if (firmware_upload_) {
        static_cast<void>(port.remove_file(firmware_path));
        if (!port.rename_file(firmware_partial_path, firmware_path)) {
            static_cast<void>(port.remove_file(firmware_partial_path));
            constexpr std::string_view error =
                "Error: failed to finalize firmware upload [/sd/firmware.bin].";
            port.send(owner_, {core::protocol::file_cancel, {error.begin(), error.end()}});
            active_ = false;
            port.release_ownership();
            return;
        }
    }

    port.send(owner_, {core::protocol::console_message,
                       path_message("Info: upload success: ", logical_path_, ".")});
    active_ = false;
    port.release_ownership();
}

void FileUpload::cancel(FileUploadPort& port) {
    constexpr std::string_view message = "Info: Upload canceled by remote!";
    port.send(owner_, {core::protocol::file_cancel, {message.begin(), message.end()}});
    cleanup(true, port);
}

void FileUpload::record_unexpected(FileUploadPort& port) {
    ++consecutive_unexpected_;
    if (consecutive_unexpected_ < packets_per_retry_cycle) {
        return;
    }
    consecutive_unexpected_ = 0U;
    emit_current_request(port);
    ++retry_cycles_;
    if (retry_cycles_ >= maximum_retry_cycles) {
        abort(excessive_retry_message, port);
    }
}

void FileUpload::emit_current_request(FileUploadPort& port) {
    switch (expected_) {
        case ExpectedPacket::md5:
            port.send(owner_, {core::protocol::file_md5, {}});
            break;
        case ExpectedPacket::geometry:
            port.send(owner_, {core::protocol::file_geometry, {}});
            break;
        case ExpectedPacket::data:
            port.send(owner_, {core::protocol::file_data, encode_u32(requested_sequence_)});
            break;
    }
}

void FileUpload::emit_timed_retry(FileUploadPort& port) {
    constexpr std::string_view message = "Info: need retry!";
    port.send(owner_, {core::protocol::file_retry, {message.begin(), message.end()}});
    ++retry_cycles_;
    if (retry_cycles_ >= maximum_retry_cycles) {
        abort(excessive_retry_message, port);
    }
}

void FileUpload::abort(std::string_view message, FileUploadPort& port) {
    port.send(owner_, {core::protocol::file_cancel, {message.begin(), message.end()}});
    cleanup(true, port);
}

void FileUpload::cleanup(bool remove_files, FileUploadPort& port) {
    port.close_files();
    if (remove_files) {
        static_cast<void>(port.remove_file(target_path_));
        static_cast<void>(port.remove_file(md5_path_));
    }
    active_ = false;
    port.release_ownership();
}

void FileUpload::reset_retry_counters() {
    consecutive_unexpected_ = 0U;
    retry_cycles_ = 0U;
}

}  // namespace firmware::application
