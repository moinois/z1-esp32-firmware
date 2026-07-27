// Declares reusable ownership and operations for the mounted POSIX VFS.
#pragma once

#include "firmware/core/bytes.hpp"

#include <cstdio>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace firmware::target {

class PosixFile {
public:
    PosixFile() = default;
    ~PosixFile();
    PosixFile(const PosixFile&) = delete;
    PosixFile& operator=(const PosixFile&) = delete;

    bool open(std::string_view path, const char* mode);
    void close();
    bool flush_and_sync();
    bool rewind();
    std::optional<std::uint64_t> size();
    std::optional<firmware::core::ByteVector> read(std::size_t maximum_size);
    std::optional<firmware::core::ByteVector> read_at(
        std::uint64_t offset, std::size_t maximum_size);
    bool write_all(firmware::core::BytesView data);
    std::FILE* get() const;

private:
    std::FILE* file_ = nullptr;
    std::string path_;
};

bool posix_regular_file(std::string_view path);
std::optional<std::uint64_t> posix_file_size(std::string_view path);
std::optional<firmware::core::ByteVector> read_posix_file(
    std::string_view path, std::size_t maximum_size);
std::optional<std::string> calculate_posix_md5(
    std::string_view path, std::size_t block_size = 1024U);
bool create_posix_parent_directories(std::string_view path, std::uint32_t mode);

}  // namespace firmware::target
