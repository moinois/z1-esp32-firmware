// Declares TCP-origin bytewise configuration-copy file operations.
#pragma once

#include "firmware/application/configuration_files.hpp"

namespace firmware::application {
class TcpClientSession;
}

namespace firmware::target {

// Implements configuration copy I/O over POSIX VFS and a TCP response queue.
class TcpConfigurationFileAdapter final
    : public firmware::application::ConfigurationFilePort {
public:
    std::string_view active_configuration_path() const override;
    std::string_view default_configuration_path() const override;
    // Binds copy responses to the originating TCP session.
    explicit TcpConfigurationFileAdapter(
        firmware::application::TcpClientSession& session);
    ~TcpConfigurationFileAdapter() override;

    bool file_exists(std::string_view path) override;
    bool open_source(std::string_view path) override;
    bool open_truncated_destination(std::string_view path) override;
    firmware::application::ByteRead read_byte() override;
    bool write_byte(std::uint8_t value) override;
    void close_source() override;
    bool close_destination() override;
    void send(firmware::core::Frame frame) override;

private:
    firmware::application::TcpClientSession& session_;
    FILE* source_ = nullptr;
    FILE* destination_ = nullptr;
};

}  // namespace firmware::target
