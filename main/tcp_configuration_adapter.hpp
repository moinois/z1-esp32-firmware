// Declares TCP-origin live configuration and SD persistence operations.
#pragma once

#include "firmware/application/configuration_get.hpp"
#include "firmware/application/configuration_set.hpp"

#include <cstdio>

namespace firmware::application {
class TcpClientSession;
}

namespace firmware::target {

// Implements both live configuration ports over POSIX VFS and TCP responses.
class TcpConfigurationAdapter final
    : public firmware::application::ConfigurationGetPort,
      public firmware::application::ConfigurationSetPort {
public:
    // Binds configuration responses to the originating TCP session.
    explicit TcpConfigurationAdapter(firmware::application::TcpClientSession& session);
    ~TcpConfigurationAdapter() override;

    std::optional<std::vector<firmware::core::ByteVector>>
    read_configuration_chunks(std::size_t maximum_chunk_size) override;
    std::optional<std::string> read_value(std::string_view tag,
                                          std::string_view key) override;
    void send(firmware::core::Frame frame) override;
    bool set_value(std::string_view tag, std::string_view key,
                   std::string_view value) override;

private:
    firmware::application::TcpClientSession& session_;
};

}  // namespace firmware::target
