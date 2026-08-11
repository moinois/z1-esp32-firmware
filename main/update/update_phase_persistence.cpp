/** @file @brief Persists update phase and emits exact DIAG-032 failures. */
#include "update_phase_persistence.hpp"
#include "nvs_key_value_adapter.hpp"
#include "esp_log.h"
#include "application/diagnostics/update_diagnostics.hpp"

namespace firmware::target {
bool persist_update_phase(std::uint8_t phase) {
    const auto result = NvsKeyValueAdapter{}.write_u8_detailed(
        "ota_state", "phase", phase);
    if (result.succeeded()) return true;
    if (result.stage == NvsMutationStage::open) {
        const auto message = firmware::application::update_nvs_open_failure(
            esp_err_to_name(result.error));
        ESP_LOGW("app_upgrade", "%s", message.c_str());
    } else {
        const auto message = firmware::application::update_nvs_save_failure(
            phase, esp_err_to_name(result.error));
        ESP_LOGW("app_upgrade", "%s", message.c_str());
    }
    return false;
}
}  // namespace firmware::target
