/** @file @brief Declares POSIX directory and MD5 query ports for one TCP client session. */
#pragma once

#include "application/storage/directory_listing.hpp"
#include "application/storage/file_hash_command.hpp"

namespace firmware::application {
class TcpClientSession;
}

namespace firmware::target {

/** Implements directory-list queries with TCP origin-aware response routing. */
class TcpDirectoryListAdapter final
    : public firmware::application::DirectoryListPort {
public:
    /// Binds directory results to the originating TCP session.
    explicit TcpDirectoryListAdapter(firmware::application::TcpClientSession& session);

    std::optional<std::vector<firmware::application::DirectoryEntry>> list_directory(
        std::string_view path) override;
    void send(firmware::core::Frame frame) override;

private:
    firmware::application::TcpClientSession& session_;
};

/** Implements bounded file hashing and returns results to one TCP session. */
class TcpFileHashAdapter final : public firmware::application::FileHashPort {
public:
    /// Binds filesystem classification, MD5 calculation, and responses to TCP.
    explicit TcpFileHashAdapter(firmware::application::TcpClientSession& session);

    firmware::application::FileHashPathState inspect_path(
        std::string_view path) override;
    std::optional<std::string> calculate_md5(
        std::string_view path, std::size_t block_size) override;
    void send(firmware::core::Frame frame) override;

private:
    firmware::application::TcpClientSession& session_;
};

}  // namespace firmware::target
