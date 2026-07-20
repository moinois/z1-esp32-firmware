// Defines the host upload state machine and its replaceable external port.
#pragma once

#include "firmware/application/ownership.hpp"
#include "firmware/core/file_transfer_paths.hpp"
#include "firmware/core/frame.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace firmware::application {

// Isolates upload policy from filesystem, transport, and ownership adapters.
class FileUploadPort {
public:
    // Enables safe destruction through a substituted port implementation.
    virtual ~FileUploadPort() = default;

    // Performs best-effort preparation of mapped cache base directories.
    virtual void prepare_cache_paths(const core::FileCachePaths& paths) = 0;

    // Creates all parents for a sidecar using the supplied permission mode.
    virtual bool create_parent_directories(std::string_view path,
                                           std::uint32_t mode) = 0;

    // Creates or truncates the primary upload target.
    virtual bool open_primary(std::string_view path) = 0;

    // Creates or truncates the mapped MD5 sidecar.
    virtual bool open_md5(std::string_view path) = 0;

    // Appends one accepted data payload to the primary target.
    virtual bool write_primary(core::BytesView data) = 0;

    // Writes the announced checksum bytes without controlling the exchange.
    virtual bool write_md5(core::BytesView data) = 0;

    // Closes both streams during cancellation or failure cleanup.
    virtual void close_files() = 0;

    // Flushes and closes both streams after the completion acknowledgement.
    virtual void flush_and_close() = 0;

    // Attempts best-effort removal of one file path.
    virtual bool remove_file(std::string_view path) = 0;

    // Renames a completed firmware partial file into its final path.
    virtual bool rename_file(std::string_view source, std::string_view destination) = 0;

    // Sends one response to the start request's retained connection identity.
    virtual void send(const HostIdentity& host, core::Frame frame) = 0;

    // Releases logical file-transfer ownership after a terminal outcome.
    virtual void release_ownership() = 0;
};

// Processes host-to-mainboard upload after admission selects a valid start.
class FileUpload {
public:
    // Opens the primary and MD5 targets and initializes protocol timing.
    bool start(const HostIdentity& owner, std::string path,
               std::uint64_t now_milliseconds, FileUploadPort& port);

    // Processes one latest-value owner transfer packet.
    void handle(const core::Frame& frame, std::uint64_t now_milliseconds,
                FileUploadPort& port);

    // Emits periodic retry or terminal inactivity responses when due.
    void poll(std::uint64_t now_milliseconds, FileUploadPort& port);

    // Reports whether both upload streams remain owned by this operation.
    bool active() const;

private:
    enum class ExpectedPacket {
        md5,
        geometry,
        data,
    };

    void accept_md5(core::BytesView payload, FileUploadPort& port);
    void accept_geometry(core::BytesView payload, FileUploadPort& port);
    void accept_data(core::BytesView payload, FileUploadPort& port);
    void complete(FileUploadPort& port);
    void cancel(FileUploadPort& port);
    void record_unexpected(FileUploadPort& port);
    void emit_current_request(FileUploadPort& port);
    void emit_timed_retry(FileUploadPort& port);
    void abort(std::string_view message, FileUploadPort& port);
    void cleanup(bool remove_files, FileUploadPort& port);
    void reset_retry_counters();

    HostIdentity owner_;
    std::string logical_path_;
    std::string target_path_;
    std::string md5_path_;
    std::uint64_t last_activity_milliseconds_ = 0U;
    std::uint64_t next_timed_retry_milliseconds_ = 0U;
    std::uint32_t announced_frame_count_ = 0U;
    std::uint32_t requested_sequence_ = 1U;
    std::uint8_t consecutive_unexpected_ = 0U;
    std::uint8_t retry_cycles_ = 0U;
    ExpectedPacket expected_ = ExpectedPacket::md5;
    bool firmware_upload_ = false;
    bool active_ = false;
};

}  // namespace firmware::application
