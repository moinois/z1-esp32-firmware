/** @file @brief Implements shared POSIX VFS file ownership, bounded I/O, and hashing. */
#include "posix_file.hpp"
#include "sd_access_diagnostics.hpp"
#include "firmware/core/sd_user_path.hpp"

#include "mbedtls/md5.h"

#include <cerrno>
#include <climits>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace firmware::target {
namespace {

// Reports whether a physical path belongs to the removable SD volume.
bool sd_path(std::string_view path) {
    return path == firmware::core::sd_mount_path ||
           (path.size() >= firmware::core::sd_mount_prefix.size() &&
            path.substr(0U, firmware::core::sd_mount_prefix.size()) ==
                firmware::core::sd_mount_prefix);
}

}  // namespace

PosixFile::~PosixFile() { close(); }

bool PosixFile::open(std::string_view path, const char* mode) {
    close();
    path_ = path;
    file_ = std::fopen(std::string(path).c_str(), mode);
    if (file_ == nullptr && sd_path(path)) {
        log_sd_access_failure("open file", path, errno);
    }
    return file_ != nullptr;
}

void PosixFile::close() {
    if (file_ != nullptr) std::fclose(file_);
    file_ = nullptr;
    path_.clear();
}

bool PosixFile::flush_and_sync() {
    if (file_ != nullptr && std::fflush(file_) == 0 &&
        fsync(fileno(file_)) == 0) return true;
    if (!path_.empty()) log_sd_access_failure("flush file", path_, errno);
    return false;
}

bool PosixFile::rewind() {
    if (file_ != nullptr && std::fseek(file_, 0L, SEEK_SET) == 0) return true;
    if (!path_.empty()) log_sd_access_failure("rewind file", path_, errno);
    return false;
}

std::optional<std::uint64_t> PosixFile::size() {
    if (file_ == nullptr || std::fseek(file_, 0L, SEEK_END) != 0) {
        if (!path_.empty()) log_sd_access_failure("size file", path_, errno);
        return std::nullopt;
    }
    const long value = std::ftell(file_);
    if (value < 0L || !rewind()) return std::nullopt;
    return static_cast<std::uint64_t>(value);
}

std::optional<firmware::core::ByteVector> PosixFile::read(
    std::size_t maximum_size) {
    if (file_ == nullptr) return std::nullopt;
    firmware::core::ByteVector data(maximum_size);
    const std::size_t count = std::fread(data.data(), 1U, maximum_size, file_);
    if (std::ferror(file_) != 0) {
        log_sd_access_failure("read file", path_, errno);
        return std::nullopt;
    }
    data.resize(count);
    return data;
}

std::optional<firmware::core::ByteVector> PosixFile::read_at(
    std::uint64_t offset, std::size_t maximum_size) {
    if (file_ == nullptr || offset > static_cast<std::uint64_t>(LONG_MAX) ||
        std::fseek(file_, static_cast<long>(offset), SEEK_SET) != 0) {
        if (!path_.empty()) log_sd_access_failure("seek file", path_, errno);
        return std::nullopt;
    }
    return read(maximum_size);
}

bool PosixFile::write_all(firmware::core::BytesView data) {
    if (file_ == nullptr) return false;
    std::size_t written = 0U;
    while (written < data.size()) {
        const std::size_t count =
            std::fwrite(data.data() + written, 1U, data.size() - written, file_);
        if (count == 0U) {
            log_sd_access_failure("write file", path_, errno);
            return false;
        }
        written += count;
    }
    return true;
}

std::FILE* PosixFile::get() const { return file_; }

bool posix_regular_file(std::string_view path) {
    struct stat information{};
    return stat(std::string(path).c_str(), &information) == 0 &&
           S_ISREG(information.st_mode);
}

std::optional<std::uint64_t> posix_file_size(std::string_view path) {
    struct stat information{};
    if (stat(std::string(path).c_str(), &information) != 0 ||
        !S_ISREG(information.st_mode) || information.st_size < 0) {
        return std::nullopt;
    }
    return static_cast<std::uint64_t>(information.st_size);
}

std::optional<firmware::core::ByteVector> read_posix_file(
    std::string_view path, std::size_t maximum_size) {
    PosixFile file;
    if (!file.open(path, "rb")) return std::nullopt;
    return file.read(maximum_size);
}

std::optional<std::string> calculate_posix_md5(std::string_view path,
                                               std::size_t block_size) {
    PosixFile file;
    if (block_size == 0U || !file.open(path, "rb")) return std::nullopt;
    std::vector<std::uint8_t> buffer(block_size);
    mbedtls_md5_context context;
    mbedtls_md5_init(&context);
    mbedtls_md5_starts(&context);
    while (true) {
        const std::size_t count =
            std::fread(buffer.data(), 1U, buffer.size(), file.get());
        if (count == 0U) break;
        mbedtls_md5_update(&context, buffer.data(), count);
    }
    if (std::ferror(file.get()) != 0) {
        mbedtls_md5_free(&context);
        return std::nullopt;
    }
    std::uint8_t digest[16];
    mbedtls_md5_finish(&context, digest);
    mbedtls_md5_free(&context);
    static constexpr char hex[] = "0123456789abcdef";
    std::string result(32U, '0');
    for (std::size_t index = 0U; index < 16U; ++index) {
        result[index * 2U] = hex[digest[index] >> 4U];
        result[index * 2U + 1U] = hex[digest[index] & 0x0fU];
    }
    return result;
}

bool create_posix_parent_directories(std::string_view path, std::uint32_t mode) {
    std::string value(path);
    const std::size_t slash = value.find_last_of('/');
    if (slash == std::string::npos) return true;
    value.resize(slash);
    for (std::size_t position = 1U; position <= value.size(); ++position) {
        if (position != value.size() && value[position] != '/') continue;
        const std::string part = value.substr(0U, position);
        if (mkdir(part.c_str(), static_cast<mode_t>(mode)) != 0 && errno != EEXIST) {
            return false;
        }
    }
    return true;
}

}  // namespace firmware::target
