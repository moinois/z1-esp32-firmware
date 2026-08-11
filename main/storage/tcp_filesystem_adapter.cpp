/** @file @brief Implements POSIX filesystem operations and TCP-origin response routing. */
#include "tcp_filesystem_adapter.hpp"
#include "sd_access_diagnostics.hpp"
#include "posix_filesystem_mutation.hpp"

#include "application/transport/tcp_client_session.hpp"
#include "esp_log.h"

#include <cerrno>
#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace firmware::target {

TcpFilesystemAdapter::TcpFilesystemAdapter(
    firmware::application::TcpClientSession& session)
    : session_(session) {}

bool TcpFilesystemAdapter::create_directory(std::string_view path,
                                            std::uint32_t mode) {
    return create_posix_directory(path, mode);
}

void TcpFilesystemAdapter::remove_recursively(std::string_view path) {
    remove_posix_tree(path);
}

bool TcpFilesystemAdapter::path_exists(std::string_view path) {
    struct stat status{};
    const std::string value(path);
    return stat(value.c_str(), &status) == 0;
}

bool TcpFilesystemAdapter::rename_path(std::string_view source,
                                       std::string_view destination) {
    return rename_posix_path(source, destination);
}

void TcpFilesystemAdapter::send(firmware::core::Frame frame) {
    static_cast<void>(session_.queue_frame(frame));
}

void TcpFilesystemAdapter::log_warning(std::string_view message) {
    ESP_LOGW("APP_FILE", "%.*s", static_cast<int>(message.size()),
             message.data());
}

}  // namespace firmware::target
