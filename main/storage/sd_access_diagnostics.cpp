/** @file @brief Implements shared SD mount state and APP_FILE failure diagnostics. */
#include "sd_access_diagnostics.hpp"

#include "esp_log.h"
#include "core/filesystem/sd_access_diagnostic.hpp"

#include <atomic>

namespace firmware::target {
namespace {

std::atomic_bool mounted{false};
constexpr char tag[] = "APP_FILE";

}  // namespace

void set_sd_storage_mounted(bool value) {
    mounted.store(value, std::memory_order_release);
}

bool sd_storage_mounted() {
    return mounted.load(std::memory_order_acquire);
}

void log_sd_access_failure(std::string_view operation, std::string_view path,
                           int error_number) {
    const std::string_view reason = firmware::core::sd_access_failure_reason(
        sd_storage_mounted(), error_number);
    ESP_LOGE(tag, "SD access failed: operation=%.*s path=%.*s reason=%.*s errno=%d",
             static_cast<int>(operation.size()), operation.data(),
             static_cast<int>(path.size()), path.data(),
             static_cast<int>(reason.size()), reason.data(), error_number);
}

}  // namespace firmware::target
