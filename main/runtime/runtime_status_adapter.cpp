/** @file @brief Implements target runtime status sources and the shared snapshot store. */
#include "runtime_status_adapter.hpp"

#include "recording_request_state.hpp"
#include "runtime_play_observer.hpp"

#include "esp_vfs_fat.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "firmware/application/router.hpp"
#include "firmware/application/update_phase.hpp"
#include "firmware/application/recording_policy.hpp"
#include "firmware/core/sd_user_path.hpp"

#include <atomic>

namespace firmware::target {
namespace {
firmware::application::ControllerSnapshots snapshots;
std::atomic_uint8_t update_phase{0U};
std::atomic_uint8_t update_progress{0U};
esp_timer_handle_t controller_success_timer = nullptr;

void clear_controller_success_status(void*) {
    update_progress.store(0U, std::memory_order_release);
    update_phase.store(0U, std::memory_order_release);
}
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
    return firmware::application::recording_conditions_active(
        recording_requested(), streamed_play_running(), controller_running());
}

std::optional<firmware::application::SdCapacity>
RuntimeStatusAdapter::sd_capacity() const {
    std::uint64_t total_bytes = 0U;
    std::uint64_t free_bytes = 0U;
    if (esp_vfs_fat_info(firmware::core::sd_mount_path.data(), &total_bytes,
                         &free_bytes) != ESP_OK) {
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
    if (phase == 4U) {
        if (controller_success_timer == nullptr) {
            const esp_timer_create_args_t arguments{
                .callback = clear_controller_success_status,
                .arg = nullptr,
                .dispatch_method = ESP_TIMER_TASK,
                .name = "controller_success",
                .skip_unhandled_events = false,
            };
            if (esp_timer_create(&arguments, &controller_success_timer) != ESP_OK) {
                return;
            }
        }
        static_cast<void>(esp_timer_stop(controller_success_timer));
        static_cast<void>(esp_timer_start_once(controller_success_timer, 3000000U));
    }
}

}  // namespace firmware::target
