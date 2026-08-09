/** @file @brief Declares ESP-IDF OTA and staged-controller operations for update application. */
#pragma once

#include "application/update/update_application.hpp"

#include "esp_ota_ops.h"

namespace firmware::target {

/// Owns one inactive OTA partition and its active write handle.
class OtaUpdateAdapter final : public firmware::application::UpdateApplicationPort {
public:
    void publish_phase(std::uint8_t phase) override;
    bool select_inactive_partition() override;
    /// Erases the complete selected partition for a direct web update.
    bool erase_inactive_partition();
    bool begin_mainboard_write(std::uint32_t size) override;
    bool write_mainboard(firmware::core::BytesView image) override;
    bool finalize_mainboard_write() override;
    bool select_mainboard_for_boot() override;
    void abort_mainboard_write() override;
    void stage_controller(std::string_view path,
                          firmware::core::BytesView image) override;
    void persist_phase_direct(std::uint8_t phase) override;
    void remove_aggregate(std::string_view path) override;
    void send_controller_reset() override;
    void restart_mainboard() override;

private:
    const esp_partition_t* inactive_partition_ = nullptr;
    esp_ota_handle_t ota_handle_ = 0U;
    bool ota_active_ = false;
};

}  // namespace firmware::target
