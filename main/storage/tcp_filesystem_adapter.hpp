/** @file @brief Declares a POSIX filesystem port whose responses target one TCP session. */
#pragma once

#include "firmware/application/filesystem_commands.hpp"

namespace firmware::application {
class TcpClientSession;
}

namespace firmware::target {

/** Executes filesystem mutations and returns framed results to one TCP origin. */
class TcpFilesystemAdapter final
    : public firmware::application::FilesystemCommandPort {
public:
    /// Binds filesystem mutation responses to the originating TCP session.
    explicit TcpFilesystemAdapter(firmware::application::TcpClientSession& session);

    bool create_directory(std::string_view path, std::uint32_t mode) override;
    void remove_recursively(std::string_view path) override;
    bool path_exists(std::string_view path) override;
    bool rename_path(std::string_view source,
                     std::string_view destination) override;
    void send(firmware::core::Frame frame) override;

private:
    firmware::application::TcpClientSession& session_;
};

}  // namespace firmware::target
