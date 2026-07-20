// Implements TCP-origin play preparation over the mounted POSIX VFS.
#include "tcp_play_adapter.hpp"

#include "firmware/application/tcp_client_session.hpp"
#include "firmware/core/file_transfer_paths.hpp"

#include <cstdio>

namespace firmware::target {

TcpPlayPreparationAdapter::TcpPlayPreparationAdapter(
    firmware::application::TcpClientSession& session) : session_(session) {}

TcpPlayPreparationAdapter::~TcpPlayPreparationAdapter() {
    close_file();
}

void TcpPlayPreparationAdapter::close_file() {
    if (file_ != nullptr) std::fclose(file_);
    file_ = nullptr;
}

std::optional<std::uint64_t> TcpPlayPreparationAdapter::open_file(
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
    return static_cast<std::uint64_t>(size);
}

std::optional<std::string> TcpPlayPreparationAdapter::cached_md5(
    std::string_view path) {
    const auto cache = firmware::core::map_file_cache_paths(path).md5_path;
    if (!cache.has_value()) return std::nullopt;
    std::FILE* input = std::fopen(cache->c_str(), "rb");
    if (input == nullptr) return std::nullopt;
    std::uint8_t bytes[63]{};
    const std::size_t count = std::fread(bytes, 1U, sizeof(bytes), input);
    std::fclose(input);
    return firmware::core::extract_cached_md5({bytes, count});
}

void TcpPlayPreparationAdapter::broadcast(firmware::core::Frame frame) {
    static_cast<void>(session_.queue_frame(frame));
}

}  // namespace firmware::target
