// Implements upload/download file effects through POSIX VFS and mbedTLS MD5.
#include "tcp_file_transfer_adapter.hpp"

#include "esp_heap_caps.h"
#include "firmware/application/tcp_client_session.hpp"

#include "mbedtls/md5.h"

#include <cstdio>
#include <cerrno>
#include <climits>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

namespace firmware::target {
namespace {

// Creates each missing directory component without requiring shell utilities.
bool make_parent_directories(std::string_view path, std::uint32_t mode) {
    std::string value(path);
    const std::size_t final_slash = value.find_last_of('/');
    if (final_slash == std::string::npos) return true;
    value.resize(final_slash);
    for (std::size_t position = 1U; position <= value.size(); ++position) {
        if (position != value.size() && value[position] != '/') continue;
        const std::string part = value.substr(0U, position);
        if (mkdir(part.c_str(), static_cast<mode_t>(mode)) != 0 && errno != EEXIST) {
            return false;
        }
    }
    return true;
}

bool write_bytes(std::FILE* file, firmware::core::BytesView data) {
    if (file == nullptr) return false;
    std::size_t written = 0U;
    while (written < data.size()) {
        const std::size_t count =
            std::fwrite(data.data() + written, 1U, data.size() - written, file);
        if (count == 0U) return false;
        written += count;
    }
    return true;
}

}  // namespace

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
    return make_parent_directories(path, mode);
}

bool TcpFileUploadAdapter::open_primary(std::string_view path) {
    primary_ = std::fopen(std::string(path).c_str(), "wb");
    return primary_ != nullptr;
}

bool TcpFileUploadAdapter::open_md5(std::string_view path) {
    md5_ = std::fopen(std::string(path).c_str(), "wb");
    return md5_ != nullptr;
}

bool TcpFileUploadAdapter::write_primary(firmware::core::BytesView data) {
    return write_bytes(primary_, data);
}

bool TcpFileUploadAdapter::write_md5(firmware::core::BytesView data) {
    return write_bytes(md5_, data);
}

void TcpFileUploadAdapter::close_files() {
    if (primary_ != nullptr) std::fclose(primary_);
    if (md5_ != nullptr) std::fclose(md5_);
    primary_ = nullptr;
    md5_ = nullptr;
}

void TcpFileUploadAdapter::flush_and_close() {
    if (primary_ != nullptr) {
        std::fflush(primary_);
        fsync(fileno(primary_));
    }
    if (md5_ != nullptr) {
        std::fflush(md5_);
        fsync(fileno(md5_));
    }
    close_files();
}

bool TcpFileUploadAdapter::remove_file(std::string_view path) {
    return unlink(std::string(path).c_str()) == 0;
}

bool TcpFileUploadAdapter::rename_file(std::string_view source,
                                       std::string_view destination) {
    return rename(std::string(source).c_str(), std::string(destination).c_str()) == 0;
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
    if (paths.md5_path.has_value()) make_parent_directories(*paths.md5_path, 0777U);
    if (paths.compressed_path.has_value()) {
        make_parent_directories(*paths.compressed_path, 0777U);
    }
}

std::optional<std::string> TcpFileDownloadAdapter::calculate_md5(
    std::string_view path) {
    std::FILE* input = std::fopen(std::string(path).c_str(), "rb");
    if (input == nullptr) return std::nullopt;
    mbedtls_md5_context context;
    mbedtls_md5_init(&context);
    mbedtls_md5_starts(&context);
    std::uint8_t buffer[1024];
    while (const std::size_t count = std::fread(buffer, 1U, sizeof(buffer), input)) {
        mbedtls_md5_update(&context, buffer, count);
    }
    if (std::ferror(input) != 0) {
        std::fclose(input);
        mbedtls_md5_free(&context);
        return std::nullopt;
    }
    std::uint8_t digest[16];
    mbedtls_md5_finish(&context, digest);
    mbedtls_md5_free(&context);
    std::fclose(input);
    static constexpr char hex[] = "0123456789abcdef";
    std::string output(32U, '0');
    for (std::size_t i = 0U; i < 16U; ++i) {
        output[i * 2U] = hex[digest[i] >> 4U];
        output[i * 2U + 1U] = hex[digest[i] & 0x0fU];
    }
    return output;
}

std::optional<firmware::core::ByteVector> TcpFileDownloadAdapter::read_cache(
    std::string_view path, std::size_t maximum_size) {
    std::FILE* input = std::fopen(std::string(path).c_str(), "rb");
    if (input == nullptr) return std::nullopt;
    firmware::core::ByteVector data(maximum_size);
    const std::size_t count = std::fread(data.data(), 1U, maximum_size, input);
    data.resize(count);
    std::fclose(input);
    return data;
}

bool TcpFileDownloadAdapter::file_exists(std::string_view path) {
    struct stat information{};
    return stat(std::string(path).c_str(), &information) == 0 &&
           S_ISREG(information.st_mode);
}

std::optional<std::uint64_t> TcpFileDownloadAdapter::open_file(
    std::string_view path) {
    close_file();
    file_ = std::fopen(std::string(path).c_str(), "rb");
    if (file_ == nullptr || std::fseek(file_, 0L, SEEK_END) != 0) {
        close_file();
        return std::nullopt;
    }
    const long size = std::ftell(file_);
    if (size < 0L || std::fseek(file_, 0L, SEEK_SET) != 0) {
        close_file();
        return std::nullopt;
    }
    return std::optional<std::uint64_t>(size);
}

std::optional<firmware::core::ByteVector> TcpFileDownloadAdapter::read_file(
    std::uint64_t offset, std::size_t maximum_size) {
    if (file_ == nullptr || offset > static_cast<std::uint64_t>(LONG_MAX) ||
        std::fseek(file_, static_cast<long>(offset), SEEK_SET) != 0) {
        return std::nullopt;
    }
    firmware::core::ByteVector data(maximum_size);
    const std::size_t count = std::fread(data.data(), 1U, maximum_size, file_);
    if (std::ferror(file_) != 0) return std::nullopt;
    data.resize(count);
    return data;
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
    if (file_ != nullptr) std::fclose(file_);
    file_ = nullptr;
}

void TcpFileDownloadAdapter::send(const firmware::application::HostIdentity&,
                                  firmware::core::Frame frame) {
    static_cast<void>(session_.queue_frame(frame));
}

void TcpFileDownloadAdapter::release_ownership() { close_file(); }

}  // namespace firmware::target
