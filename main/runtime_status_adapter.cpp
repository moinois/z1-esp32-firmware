// Implements target runtime status sources and the shared snapshot store.
#include "runtime_status_adapter.hpp"

#include "recording_request_state.hpp"
#include "runtime_play_observer.hpp"

#include "esp_vfs_fat.h"
#include "firmware/application/router.hpp"
#include "firmware/application/update_phase.hpp"

namespace firmware::target {
namespace {
firmware::application::ControllerSnapshots snapshots;
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
    return {};
}

std::optional<std::int32_t> RuntimeStatusAdapter::station_rssi() const {
    return std::nullopt;
}

firmware::application::ControllerSnapshots& shared_controller_snapshots() {
    return snapshots;
}

}  // namespace firmware::target
