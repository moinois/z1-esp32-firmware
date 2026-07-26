// Declares TCP-origin file preparation for the shared streamed-play session.
#pragma once

#include "firmware/application/play_session.hpp"
#include "posix_file.hpp"

namespace firmware::application {
class TcpClientSession;
}

namespace firmware::target {

// Implements preparation-only play I/O while retaining the TCP response origin.
class TcpPlayPreparationAdapter final
    : public firmware::application::PlayPreparationPort {
public:
    // Binds preparation failures and success responses to one TCP session.
    explicit TcpPlayPreparationAdapter(
        firmware::application::TcpClientSession& session);
    ~TcpPlayPreparationAdapter() override;

    void close_file() override;
    std::optional<std::uint64_t> open_file(std::string_view path) override;
    std::optional<std::string> cached_md5(std::string_view path) override;
    void broadcast(firmware::core::Frame frame) override;

private:
    firmware::application::TcpClientSession& session_;
    PosixFile file_;
};

}  // namespace firmware::target
