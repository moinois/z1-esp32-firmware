// Implements streamed-play file access, checksum lookup, and UART responses.
#include "controller_play_adapter.hpp"
#include "esp_log.h"

#include "controller_uart_adapter.hpp"
#include "runtime_play_observer.hpp"
#include "tcp_control_adapter.hpp"

#include "esp_timer.h"
#include "firmware/core/file_transfer_paths.hpp"
#include "firmware/core/frame.hpp"

#include <cstring>
#include <sys/stat.h>

namespace firmware::target {

ControllerPlayAdapter::ControllerPlayAdapter(ControllerUartAdapter& uart)
    : uart_(uart) {}

ControllerPlayAdapter::~ControllerPlayAdapter() {
    close_file();
}

void ControllerPlayAdapter::close_file() {
    if (file_ != nullptr) std::fclose(file_);
    file_ = nullptr;
}

std::optional<std::uint64_t> ControllerPlayAdapter::open_file(
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

std::optional<std::string> ControllerPlayAdapter::cached_md5(
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

void ControllerPlayAdapter::broadcast(firmware::core::Frame frame) {
    static_cast<void>(send(std::move(frame)));
}

bool ControllerPlayAdapter::send(firmware::core::Frame frame) {
    const auto encoded = firmware::core::encode_frame(frame);
    if (encoded.empty()) return false;
    const int written = uart_.write(encoded);
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
    return file_ != nullptr && std::fseek(file_, 0L, SEEK_SET) == 0;
}

std::uint64_t ControllerPlayAdapter::now_milliseconds() const {
    return static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL);
}

std::optional<firmware::application::PlayLineChunk>
ControllerPlayAdapter::read_chunk(std::size_t maximum_size) {
    if (file_ == nullptr || maximum_size == 0U) return std::nullopt;
    firmware::core::ByteVector bytes(maximum_size);
    const std::size_t count = std::fread(bytes.data(), 1U, maximum_size, file_);
    const bool failed = std::ferror(file_) != 0;
    const bool end = std::feof(file_) != 0;
    if (failed) return std::nullopt;
    bytes.resize(count);
    return firmware::application::PlayLineChunk{std::move(bytes), end};
}

}  // namespace firmware::target
