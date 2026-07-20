// Implements target runtime status sources and the shared snapshot store.
#include "runtime_status_adapter.hpp"

#include "recording_request_state.hpp"
#include "runtime_play_observer.hpp"

#include "esp_vfs_fat.h"
#include "esp_wifi.h"
#include "firmware/application/router.hpp"
#include "firmware/application/update_phase.hpp"

#include <atomic>

namespace firmware::target {
namespace {
firmware::application::ControllerSnapshots snapshots;
std::atomic_uint8_t update_phase{0U};
std::atomic_uint8_t update_progress{0U};
}  // namespace

RuntimeStatusAdapter::RuntimeStatusAdapter(
    firmware::application::Router& router) : router_(router) {}

bool RuntimeStatusAdapter::host_transfer_active() const {
    return router_.ownership().has_file_owner();
}

bool RuntimeStatusAdapter::recording_requested() const {
    return RecordingRequestState{}.requested();
}

bool RuntimeStatusAdapter::recording_active() const {
    return recording_requested() && streamed_play_running();
}

std::optional<firmware::application::SdCapacity>
RuntimeStatusAdapter::sd_capacity() const {
    std::uint64_t total_bytes = 0U;
    std::uint64_t free_bytes = 0U;
    if (esp_vfs_fat_info("/sd", &total_bytes, &free_bytes) != ESP_OK) {
        return std::nullopt;
    }
    constexpr std::uint64_t bytes_per_mib = 1024U * 1024U;
    return firmware::application::SdCapacity{
        total_bytes / bytes_per_mib, free_bytes / bytes_per_mib};
}

firmware::application::UpdateStatus RuntimeStatusAdapter::update_status() const {
    return {update_phase.load(std::memory_order_acquire),
            update_progress.load(std::memory_order_acquire)};
}

std::optional<std::int32_t> RuntimeStatusAdapter::station_rssi() const {
    wifi_ap_record_t access_point{};
    if (esp_wifi_sta_get_ap_info(&access_point) != ESP_OK) {
        return std::nullopt;
    }
    return static_cast<std::int32_t>(access_point.rssi);
}

firmware::application::ControllerSnapshots& shared_controller_snapshots() {
    return snapshots;
}

void publish_runtime_update_phase(std::uint8_t phase) {
    update_phase.store(phase, std::memory_order_release);
}

void publish_controller_transfer_status(std::uint8_t phase,
                                        std::uint32_t progress) {
    update_progress.store(
        static_cast<std::uint8_t>(progress > 100U ? 100U : progress),
        std::memory_order_release);
    update_phase.store(phase, std::memory_order_release);
}

}  // namespace firmware::target
