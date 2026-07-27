// Implements upload/download file effects through POSIX VFS and mbedTLS MD5.
#include "tcp_file_transfer_adapter.hpp"
#include "sd_access_diagnostics.hpp"

#include "esp_heap_caps.h"
#include "firmware/application/tcp_client_session.hpp"

#include <cstdio>
#include <cerrno>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

namespace firmware::target {
TcpFileUploadAdapter::TcpFileUploadAdapter(
    firmware::application::TcpClientSession& session) : session_(session) {}

void TcpFileUploadAdapter::prepare_cache_paths(
    const firmware::core::FileCachePaths& paths) {
    if (paths.md5_path.has_value()) create_parent_directories(*paths.md5_path, 0777U);
    if (paths.compressed_path.has_value()) {
        create_parent_directories(*paths.compressed_path, 0777U);
    }
}

bool TcpFileUploadAdapter::create_parent_directories(std::string_view path,
                                                     std::uint32_t mode) {
    if (create_posix_parent_directories(path, mode)) return true;
    log_sd_access_failure("create parent directory", path, errno);
    return false;
}

bool TcpFileUploadAdapter::open_primary(std::string_view path) {
    return primary_.open(path, "wb");
}

bool TcpFileUploadAdapter::open_md5(std::string_view path) {
    return md5_.open(path, "wb");
}

bool TcpFileUploadAdapter::write_primary(firmware::core::BytesView data) {
    return primary_.write_all(data);
}

bool TcpFileUploadAdapter::write_md5(firmware::core::BytesView data) {
    return md5_.write_all(data);
}

void TcpFileUploadAdapter::close_files() {
    primary_.close();
    md5_.close();
}

void TcpFileUploadAdapter::flush_and_close() {
    if (primary_.get() != nullptr) static_cast<void>(primary_.flush_and_sync());
    if (md5_.get() != nullptr) static_cast<void>(md5_.flush_and_sync());
    close_files();
}

bool TcpFileUploadAdapter::remove_file(std::string_view path) {
    if (unlink(std::string(path).c_str()) == 0) return true;
    log_sd_access_failure("remove upload file", path, errno);
    return false;
}

bool TcpFileUploadAdapter::rename_file(std::string_view source,
                                       std::string_view destination) {
    if (rename(std::string(source).c_str(),
               std::string(destination).c_str()) == 0) return true;
    log_sd_access_failure("rename upload file", source, errno);
    return false;
}

void TcpFileUploadAdapter::send(const firmware::application::HostIdentity&,
                                firmware::core::Frame frame) {
    static_cast<void>(session_.queue_frame(frame));
}

void TcpFileUploadAdapter::release_ownership() {}

TcpFileDownloadAdapter::TcpFileDownloadAdapter(
    firmware::application::TcpClientSession& session) : session_(session) {}

void TcpFileDownloadAdapter::prepare_cache_paths(
    const firmware::core::FileCachePaths& paths) {
    if (paths.md5_path.has_value()) {
        create_posix_parent_directories(*paths.md5_path, 0777U);
    }
    if (paths.compressed_path.has_value()) {
        create_posix_parent_directories(*paths.compressed_path, 0777U);
    }
}

std::optional<std::string> TcpFileDownloadAdapter::calculate_md5(
    std::string_view path) {
    return calculate_posix_md5(path);
}

std::optional<firmware::core::ByteVector> TcpFileDownloadAdapter::read_cache(
    std::string_view path, std::size_t maximum_size) {
    return read_posix_file(path, maximum_size);
}

bool TcpFileDownloadAdapter::file_exists(std::string_view path) {
    struct stat information{};
    const int result = stat(std::string(path).c_str(), &information);
    if (result == 0 && S_ISREG(information.st_mode)) return true;
    const int error_number = result == 0 ? EINVAL : errno;
    log_sd_access_failure("inspect download file", path, error_number);
    return false;
}

std::optional<std::uint64_t> TcpFileDownloadAdapter::open_file(
    std::string_view path) {
    if (!file_.open(path, "rb")) return std::nullopt;
    return file_.size();
}

std::optional<firmware::core::ByteVector> TcpFileDownloadAdapter::read_file(
    std::uint64_t offset, std::size_t maximum_size) {
    return file_.read_at(offset, maximum_size);
}

bool TcpFileDownloadAdapter::allocate_response_workspace(std::size_t size) {
    void* workspace = heap_caps_malloc(size, MALLOC_CAP_8BIT);
    if (workspace == nullptr) {
        return false;
    }
    heap_caps_free(workspace);
    return true;
}

void TcpFileDownloadAdapter::close_file() {
    file_.close();
}

void TcpFileDownloadAdapter::send(const firmware::application::HostIdentity&,
                                  firmware::core::Frame frame) {
    static_cast<void>(session_.queue_frame(frame));
}

void TcpFileDownloadAdapter::release_ownership() { close_file(); }

}  // namespace firmware::target
