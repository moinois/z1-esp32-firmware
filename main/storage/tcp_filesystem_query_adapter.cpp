// Implements POSIX directory enumeration and mbedTLS MD5 queries.
#include "tcp_filesystem_query_adapter.hpp"
#include "posix_file.hpp"
#include "sd_access_diagnostics.hpp"

#include "firmware/application/tcp_client_session.hpp"

#include "esp_log.h"

#include <cerrno>
#include <dirent.h>
#include <cstdio>
#include <sys/stat.h>
#include <time.h>

namespace firmware::target {
namespace {

// Converts a target timestamp to the UTC metadata shape used by the service.
firmware::application::UtcFileTime make_utc_file_time(time_t value) {
    struct tm result{};
    gmtime_r(&value, &result);
    return {static_cast<std::uint16_t>(result.tm_year + 1900),
            static_cast<std::uint8_t>(result.tm_mon + 1),
            static_cast<std::uint8_t>(result.tm_mday),
            static_cast<std::uint8_t>(result.tm_hour),
            static_cast<std::uint8_t>(result.tm_min),
            static_cast<std::uint8_t>(result.tm_sec)};
}

}  // namespace

TcpDirectoryListAdapter::TcpDirectoryListAdapter(
    firmware::application::TcpClientSession& session) : session_(session) {}

std::optional<std::vector<firmware::application::DirectoryEntry>>
TcpDirectoryListAdapter::list_directory(std::string_view path) {
    const std::string root(path);
    if (!sd_storage_mounted()) {
        log_sd_access_failure("open directory", root, ENODEV);
        return std::nullopt;
    }
    DIR* directory = opendir(root.c_str());
    if (directory == nullptr) {
        const int error_number = errno;
        ESP_LOGE("APP_FILE", "Can't open dir: %s", root.c_str());
        log_sd_access_failure("open directory", root, error_number);
        return std::nullopt;
    }
    std::vector<firmware::application::DirectoryEntry> entries;
    while (const dirent* item = readdir(directory)) {
        const std::string name(item->d_name);
        if (name == "." || name == "..") continue;
        const std::string full_path = root + "/" + name;
        struct stat information{};
        const bool metadata = stat(full_path.c_str(), &information) == 0;
        entries.push_back({name,
                           metadata && S_ISDIR(information.st_mode),
                           metadata ? static_cast<std::uint64_t>(information.st_size) : 0U,
                           metadata ? make_utc_file_time(information.st_mtime)
                                    : make_utc_file_time(0),
                           metadata});
    }
    closedir(directory);
    return entries;
}

void TcpDirectoryListAdapter::send(firmware::core::Frame frame) {
    static_cast<void>(session_.queue_frame(frame));
}

TcpFileHashAdapter::TcpFileHashAdapter(
    firmware::application::TcpClientSession& session) : session_(session) {}

firmware::application::FileHashPathState TcpFileHashAdapter::inspect_path(
    std::string_view path) {
    struct stat information{};
    if (stat(std::string(path).c_str(), &information) != 0) {
        log_sd_access_failure("inspect path", path, errno);
        return firmware::application::FileHashPathState::missing;
    }
    return S_ISREG(information.st_mode)
               ? firmware::application::FileHashPathState::regular_file
               : firmware::application::FileHashPathState::not_regular;
}

std::optional<std::string> TcpFileHashAdapter::calculate_md5(
    std::string_view path, std::size_t block_size) {
    return calculate_posix_md5(path, block_size);
}

void TcpFileHashAdapter::send(firmware::core::Frame frame) {
    static_cast<void>(session_.queue_frame(frame));
}

}  // namespace firmware::target
