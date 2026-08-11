/** @file @brief Implements streamed-play file access, checksum lookup, and UART responses. */
#include "controller_play_adapter.hpp"
#include "esp_log.h"

#include "controller_channel_adapter.hpp"
#include "runtime_play_observer.hpp"
#include "tcp_control_adapter.hpp"

#include "esp_timer.h"
#include "core/filesystem/file_transfer_paths.hpp"
#include "core/protocol/frame.hpp"

#include <cstring>
#include <sys/stat.h>

namespace firmware::target {

ControllerPlayAdapter::ControllerPlayAdapter(ControllerChannelAdapter& channel)
    : channel_(channel) {}

ControllerPlayAdapter::~ControllerPlayAdapter() = default;

void ControllerPlayAdapter::close_file() {
    file_.close();
}

std::optional<std::uint64_t> ControllerPlayAdapter::open_file(
    std::string_view path) {
    if (!file_.open(path, "rb")) return std::nullopt;
    return file_.size();
}

std::optional<std::string> ControllerPlayAdapter::cached_md5(
    std::string_view path) {
    const auto cache = firmware::core::map_file_cache_paths(path).md5_path;
    if (!cache.has_value()) return std::nullopt;
    const auto bytes = read_posix_file(*cache, 63U);
    if (!bytes.has_value()) return std::nullopt;
    return firmware::core::extract_cached_md5(*bytes);
}

void ControllerPlayAdapter::broadcast(firmware::core::Frame frame) {
    static_cast<void>(send(std::move(frame)));
}

void ControllerPlayAdapter::diagnose(
    const firmware::application::PlaybackDiagnostic& diagnostic) {
    if (diagnostic.error) {
        ESP_LOGE(diagnostic.tag.data(), "%s", diagnostic.message.c_str());
    } else if (diagnostic.warning) {
        ESP_LOGW(diagnostic.tag.data(), "%s", diagnostic.message.c_str());
    } else {
        ESP_LOGI(diagnostic.tag.data(), "%s", diagnostic.message.c_str());
    }
}

bool ControllerPlayAdapter::send(firmware::core::Frame frame) {
    const auto encoded = firmware::core::encode_controller_frame(frame);
    if (encoded.empty()) return false;
    const int written = channel_.write(encoded);
    if (written != static_cast<int>(encoded.size())) {
        ESP_LOGE("uart_task", "UART send failed");
        return false;
    }
    return true;
}

void ControllerPlayAdapter::play_state_changed(bool running) {
    RuntimePlayObserver{}.play_state_changed(
        running, static_cast<unsigned long long>(now_milliseconds()));
}

void ControllerPlayAdapter::release_play_ownership() {
    tcp_router_play_ownership_release();
}

bool ControllerPlayAdapter::rewind_file() {
    return file_.rewind();
}

std::uint64_t ControllerPlayAdapter::now_milliseconds() const {
    return static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL);
}

std::optional<firmware::application::PlayLineChunk>
ControllerPlayAdapter::read_chunk(std::size_t maximum_size) {
    if (maximum_size == 0U || file_.get() == nullptr) return std::nullopt;
    auto bytes = file_.read(maximum_size);
    if (!bytes.has_value()) return std::nullopt;
    const bool end = std::feof(file_.get()) != 0;
    return firmware::application::PlayLineChunk{std::move(*bytes), end};
}

}  // namespace firmware::target
