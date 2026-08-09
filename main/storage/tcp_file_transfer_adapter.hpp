/** @file @brief Declares POSIX file-transfer ports bound to one TCP client session. */
#pragma once

#include "application/storage/file_download.hpp"
#include "application/storage/file_upload.hpp"
#include "posix_file.hpp"

#include <cstdio>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace firmware::application {
class TcpClientSession;
}

namespace firmware::target {

/// Implements upload filesystem effects and origin-preserving responses.
class TcpFileUploadAdapter final : public firmware::application::FileUploadPort {
public:
    TcpFileUploadAdapter() = default;
    void bind(firmware::application::TcpClientSession* session);
    void prepare_cache_paths(const firmware::core::FileCachePaths& paths) override;
    bool create_parent_directories(std::string_view path, std::uint32_t mode) override;
    bool open_primary(std::string_view path) override;
    bool open_md5(std::string_view path) override;
    bool write_primary(firmware::core::BytesView data) override;
    bool write_md5(firmware::core::BytesView data) override;
    void close_files() override;
    void flush_and_close() override;
    bool remove_file(std::string_view path) override;
    bool rename_file(std::string_view source, std::string_view destination) override;
    void send(const firmware::application::HostIdentity& host,
              firmware::core::Frame frame) override;
    void release_ownership() override;

private:
    firmware::application::TcpClientSession* session_ = nullptr;
    PosixFile primary_;
    PosixFile md5_;
};

/// Implements download filesystem effects and origin-preserving responses.
class TcpFileDownloadAdapter final : public firmware::application::FileDownloadPort {
public:
    TcpFileDownloadAdapter() = default;
    void bind(firmware::application::TcpClientSession* session);
    void prepare_cache_paths(const firmware::core::FileCachePaths& paths) override;
    std::optional<std::string> calculate_md5(std::string_view path) override;
    std::optional<firmware::core::ByteVector> read_cache(
        std::string_view path, std::size_t maximum_size) override;
    bool file_exists(std::string_view path) override;
    std::optional<std::uint64_t> open_file(std::string_view path) override;
    std::optional<firmware::core::ByteVector> read_file(
        std::uint64_t offset, std::size_t maximum_size) override;
    bool allocate_response_workspace(std::size_t size) override;
    void close_file() override;
    void send(const firmware::application::HostIdentity& host,
              firmware::core::Frame frame) override;
    void release_ownership() override;

private:
    firmware::application::TcpClientSession* session_ = nullptr;
    PosixFile file_;
};

}  // namespace firmware::target
