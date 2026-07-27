// Defines the host download state machine and its replaceable external port.
#pragma once

#include "firmware/application/ownership.hpp"
#include "firmware/core/file_transfer_paths.hpp"
#include "firmware/core/frame.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace firmware::application {

// Isolates download policy from filesystem, allocation, transport, and ownership.
class FileDownloadPort {
public:
    // Enables safe destruction through a substituted port implementation.
    virtual ~FileDownloadPort() = default;

    // Performs best-effort preparation of mapped cache base directories.
    virtual void prepare_cache_paths(const core::FileCachePaths& paths) = 0;

    // Calculates the actual MD5 of a logical file or reports failure.
    virtual std::optional<std::string> calculate_md5(std::string_view path) = 0;

    // Reads at most the requested bytes from a checksum sidecar.
    virtual std::optional<core::ByteVector> read_cache(std::string_view path,
                                                       std::size_t maximum_size) = 0;

    // Reports whether a mapped compressed sidecar exists.
    virtual bool file_exists(std::string_view path) = 0;

    // Opens the selected transfer file and returns its byte size.
    virtual std::optional<std::uint64_t> open_file(std::string_view path) = 0;

    // Reads at most one block from the currently open selected file.
    virtual std::optional<core::ByteVector> read_file(std::uint64_t offset,
                                                      std::size_t maximum_size) = 0;

    // Reserves the fixed response workspace needed for one data block.
    virtual bool allocate_response_workspace(std::size_t size) = 0;

    // Closes the currently open selected file.
    virtual void close_file() = 0;

    // Sends one response to the start request's retained connection identity.
    virtual void send(const HostIdentity& host, core::Frame frame) = 0;

    // Releases logical file-transfer ownership after a terminal outcome.
    virtual void release_ownership() = 0;
};

// Processes mainboard-to-host file transfer after admission selects a start.
class FileDownload {
public:
    // Resolves MD5 and selected content, opens it, and announces the checksum.
    bool start(const HostIdentity& owner, std::string path,
               std::uint64_t now_milliseconds, FileDownloadPort& port);

    // Processes one latest-value owner transfer packet.
    void handle(const core::Frame& frame, std::uint64_t now_milliseconds,
                FileDownloadPort& port);

    // Applies the strict inactivity timeout when no packet is available.
    void poll(std::uint64_t now_milliseconds, FileDownloadPort& port);

    // Reports whether a selected file remains open for this operation.
    bool active() const;

private:
    enum class LastResponse {
        none,
        md5,
        geometry,
        data,
    };

    void send_md5(FileDownloadPort& port);
    void send_geometry(FileDownloadPort& port);
    void send_data(std::uint32_t sequence, FileDownloadPort& port);
    void retry_last(FileDownloadPort& port);
    void record_unexpected(FileDownloadPort& port);
    void abort(std::string_view message, FileDownloadPort& port);
    void finish(FileDownloadPort& port);
    void send_path_error(std::string_view prefix, FileDownloadPort& port);

    HostIdentity owner_;
    std::string resolved_path_;
    std::string path_;
    std::string md5_;
    std::uint64_t file_size_ = 0U;
    std::uint64_t last_activity_milliseconds_ = 0U;
    std::uint32_t last_data_sequence_ = 0U;
    std::uint8_t unexpected_count_ = 0U;
    LastResponse last_response_ = LastResponse::none;
    core::Frame retained_response_;
    bool active_ = false;
};

}  // namespace firmware::application
