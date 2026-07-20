// Implements bounded ESP-IDF OTA writes and staged controller-image effects.
#include "ota_update_adapter.hpp"

#include "esp_log.h"
#include "esp_system.h"

#include "controller_command_loop.hpp"

#include "firmware/core/bytes.hpp"

#include <cstdio>
#include <string>

namespace firmware::target {
namespace {

constexpr char tag[] = "OTA";

}  // namespace

void OtaUpdateAdapter::publish_phase(std::uint8_t phase) {
    ESP_LOGI(tag, "update phase %u", static_cast<unsigned>(phase));
}

bool OtaUpdateAdapter::select_inactive_partition() {
    inactive_partition_ = esp_ota_get_next_update_partition(nullptr);
    return inactive_partition_ != nullptr;
}

bool OtaUpdateAdapter::begin_mainboard_write(std::uint32_t size) {
    if (inactive_partition_ == nullptr || ota_active_) {
        return false;
    }
    if (esp_ota_begin(inactive_partition_, size, &ota_handle_) != ESP_OK) {
        return false;
    }
    ota_active_ = true;
    return true;
}

bool OtaUpdateAdapter::write_mainboard(firmware::core::BytesView image) {
    return ota_active_ && esp_ota_write(ota_handle_, image.data(), image.size()) == ESP_OK;
}

bool OtaUpdateAdapter::finalize_mainboard_write() {
    if (!ota_active_ || esp_ota_end(ota_handle_) != ESP_OK) {
        return false;
    }
    ota_active_ = false;
    return true;
}

bool OtaUpdateAdapter::select_mainboard_for_boot() {
    return inactive_partition_ != nullptr &&
           esp_ota_set_boot_partition(inactive_partition_) == ESP_OK;
}

void OtaUpdateAdapter::abort_mainboard_write() {
    if (ota_active_) {
        static_cast<void>(esp_ota_abort(ota_handle_));
        ota_active_ = false;
    }
}

void OtaUpdateAdapter::stage_controller(std::string_view path,
                                         firmware::core::BytesView image) {
    std::FILE* file = std::fopen(std::string(path).c_str(), "wb");
    if (file == nullptr) {
        return;
    }
    static_cast<void>(std::fwrite(image.data(), 1U, image.size(), file));
    static_cast<void>(std::fclose(file));
}

void OtaUpdateAdapter::persist_phase_direct(std::uint8_t phase) {
    publish_phase(phase);
}

void OtaUpdateAdapter::remove_aggregate(std::string_view path) {
    static_cast<void>(std::remove(std::string(path).c_str()));
}

void OtaUpdateAdapter::send_controller_reset() {
    const firmware::core::Frame reset_frame{
        0xA2U, {'r', 'e', 's', 'e', 't', 0U}};
    if (!enqueue_controller_frame(reset_frame)) {
        ESP_LOGW(tag, "controller reset could not be queued");
    }
}

void OtaUpdateAdapter::restart_mainboard() {
    esp_restart();
}

}  // namespace firmware::target
