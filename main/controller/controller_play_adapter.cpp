/** @file @brief Implements streamed-play file access, checksum lookup, and UART responses. */
#include "controller_play_adapter.hpp"
#include "esp_log.h"

#include "controller_channel_adapter.hpp"
#include "runtime_play_observer.hpp"
#include "tcp_control_adapter.hpp"
#include "controller_command_loop.hpp"
#include "host_output_adapter.hpp"
#include "play_runtime_state.hpp"
#include "posix_file.hpp"

#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "core/filesystem/file_transfer_paths.hpp"
#include "core/protocol/frame.hpp"

#include <cstring>
#include <sys/stat.h>

namespace firmware::target {

ControllerPlayAdapter::ControllerPlayAdapter(ControllerChannelAdapter& channel)
    : channel_(channel) {}

ControllerPlayAdapter::~ControllerPlayAdapter() = default;

void ControllerPlayAdapter::close_file() {
    close_shared_play_file();
}

std::optional<std::uint64_t> ControllerPlayAdapter::open_file(
    std::string_view path) {
    return open_shared_play_file(path);
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
    // Rate-limited play errors use the catastrophic TRN-006 overflow class;
    // selection later expands an admitted broadcast in USB-before-TCP order.
    const auto result = admit_host_broadcast(
        frame,
        firmware::application::HostOutputSource::rate_limited_console_error);
    if (result == firmware::application::HostOutputAdmission::purged_at_capacity) {
        diagnose(firmware::application::playback_diagnostic(
            firmware::application::PlaybackDiagnosticEvent::host_broadcast_overflow));
    }
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
    constexpr std::size_t encoded_overhead =
        firmware::core::protocol::common_frame_overhead;
    if (!response_memory_available(frame.payload.size() + encoded_overhead)) {
        diagnose(firmware::application::playback_diagnostic(
            firmware::application::PlaybackDiagnosticEvent::frame_allocation_failed));
        return false;
    }
    const auto result = enqueue_play_controller_frame(frame);
    if (result == PlayControllerEnqueueResult::capacity_full) {
        diagnose(firmware::application::playback_diagnostic(
            firmware::application::PlaybackDiagnosticEvent::output_full));
        return false;
    }
    if (result != PlayControllerEnqueueResult::accepted) {
        diagnose(firmware::application::playback_diagnostic(
            firmware::application::PlaybackDiagnosticEvent::frame_allocation_failed));
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
    return rewind_shared_play_file();
}

std::uint64_t ControllerPlayAdapter::now_milliseconds() const {
    return static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL);
}

bool ControllerPlayAdapter::response_memory_available(std::size_t bytes) {
    void* probe = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
    if (probe == nullptr) return false;
    heap_caps_free(probe);
    return true;
}

std::optional<firmware::application::PlayLineChunk>
ControllerPlayAdapter::read_chunk(std::size_t maximum_size) {
    if (maximum_size == 0U) return std::nullopt;
    return read_shared_play_chunk(maximum_size);
}

}  // namespace firmware::target
