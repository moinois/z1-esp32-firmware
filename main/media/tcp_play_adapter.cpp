/** @file @brief Implements TCP-origin play preparation over the mounted POSIX VFS. */
#include "tcp_play_adapter.hpp"

#include "firmware/application/tcp_client_session.hpp"
#include "firmware/core/file_transfer_paths.hpp"

#include <cstdio>

namespace firmware::target {

TcpPlayPreparationAdapter::TcpPlayPreparationAdapter(
    firmware::application::TcpClientSession& session) : session_(session) {}

TcpPlayPreparationAdapter::~TcpPlayPreparationAdapter() = default;

void TcpPlayPreparationAdapter::close_file() {
    file_.close();
}

std::optional<std::uint64_t> TcpPlayPreparationAdapter::open_file(
    std::string_view path) {
    if (!file_.open(path, "rb")) return std::nullopt;
    return file_.size();
}

std::optional<std::string> TcpPlayPreparationAdapter::cached_md5(
    std::string_view path) {
    const auto cache = firmware::core::map_file_cache_paths(path).md5_path;
    if (!cache.has_value()) return std::nullopt;
    const auto bytes = read_posix_file(*cache, 63U);
    if (!bytes.has_value()) return std::nullopt;
    return firmware::core::extract_cached_md5(*bytes);
}

void TcpPlayPreparationAdapter::broadcast(firmware::core::Frame frame) {
    static_cast<void>(session_.queue_frame(frame));
}

}  // namespace firmware::target
