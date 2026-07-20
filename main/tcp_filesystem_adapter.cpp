// Implements POSIX filesystem operations and TCP-origin response routing.
#include "tcp_filesystem_adapter.hpp"

#include "firmware/application/tcp_client_session.hpp"

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
    const std::string value(path);
    return mkdir(value.c_str(), static_cast<mode_t>(mode)) == 0
        || errno == EEXIST;
}

void TcpFilesystemAdapter::remove_recursively(std::string_view path) {
    const std::string value(path);
    static_cast<void>(remove(value.c_str()));
}

bool TcpFilesystemAdapter::path_exists(std::string_view path) {
    struct stat status{};
    const std::string value(path);
    return stat(value.c_str(), &status) == 0;
}

bool TcpFilesystemAdapter::rename_path(std::string_view source,
                                       std::string_view destination) {
    const std::string old_path(source);
    const std::string new_path(destination);
    return rename(old_path.c_str(), new_path.c_str()) == 0;
}

void TcpFilesystemAdapter::send(firmware::core::Frame frame) {
    static_cast<void>(session_.queue_frame(frame));
}

}  // namespace firmware::target
