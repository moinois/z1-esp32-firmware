// Implements POSIX filesystem operations and TCP-origin response routing.
#include "tcp_filesystem_adapter.hpp"
#include "sd_access_diagnostics.hpp"

#include "firmware/application/tcp_client_session.hpp"

#include <cerrno>
#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

namespace {

// Removes a directory tree using the target VFS directory and stat APIs.
void remove_tree(const std::string& path) {
    struct stat status{};
    if (stat(path.c_str(), &status) != 0) return;
    if (!S_ISDIR(status.st_mode)) {
        static_cast<void>(unlink(path.c_str()));
        return;
    }

    DIR* directory = opendir(path.c_str());
    if (directory != nullptr) {
        while (const dirent* entry = readdir(directory)) {
            const std::string name(entry->d_name);
            if (name == "." || name == "..") continue;
            remove_tree(path + "/" + name);
        }
        closedir(directory);
    }
    static_cast<void>(rmdir(path.c_str()));
}

}  // namespace

namespace firmware::target {

TcpFilesystemAdapter::TcpFilesystemAdapter(
    firmware::application::TcpClientSession& session)
    : session_(session) {}

bool TcpFilesystemAdapter::create_directory(std::string_view path,
                                            std::uint32_t mode) {
    const std::string value(path);
    if (mkdir(value.c_str(), static_cast<mode_t>(mode)) == 0 || errno == EEXIST) {
        return true;
    }
    log_sd_access_failure("create directory", path, errno);
    return false;
}

void TcpFilesystemAdapter::remove_recursively(std::string_view path) {
    const std::string value(path);
    remove_tree(value);
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
    if (rename(old_path.c_str(), new_path.c_str()) == 0) return true;
    log_sd_access_failure("rename", source, errno);
    return false;
}

void TcpFilesystemAdapter::send(firmware::core::Frame frame) {
    static_cast<void>(session_.queue_frame(frame));
}

}  // namespace firmware::target
