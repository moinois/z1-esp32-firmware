/** @file @brief Implements TCP-origin play preparation over the mounted POSIX VFS. */
#include "tcp_play_adapter.hpp"

#include "application/transport/tcp_client_session.hpp"
#include "core/filesystem/file_transfer_paths.hpp"
#include "play_runtime_state.hpp"
#include "posix_file.hpp"
#include "esp_log.h"

#include <cstdio>

namespace firmware::target {

TcpPlayPreparationAdapter::TcpPlayPreparationAdapter(
    firmware::application::TcpClientSession& session) : session_(session) {}

TcpPlayPreparationAdapter::~TcpPlayPreparationAdapter() = default;

void TcpPlayPreparationAdapter::close_file() {
    close_shared_play_file();
}

std::optional<std::uint64_t> TcpPlayPreparationAdapter::open_file(
    std::string_view path) {
    return open_shared_play_file(path);
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

void TcpPlayPreparationAdapter::diagnose(
    const firmware::application::PlaybackDiagnostic& diagnostic) {
    if (diagnostic.error) {
        ESP_LOGE(diagnostic.tag.data(), "%s", diagnostic.message.c_str());
    } else if (diagnostic.warning) {
        ESP_LOGW(diagnostic.tag.data(), "%s", diagnostic.message.c_str());
    } else {
        ESP_LOGI(diagnostic.tag.data(), "%s", diagnostic.message.c_str());
    }
}

}  // namespace firmware::target
