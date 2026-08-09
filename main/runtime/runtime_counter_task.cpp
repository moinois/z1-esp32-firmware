/** @file @brief Implements periodic runtime-counter persistence over FreeRTOS timing. */
#include "runtime_counter_task.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "application/runtime/runtime_counters.hpp"
#include "nvs_runtime_counter_adapter.hpp"

#include <ctime>
#include <cstdint>

namespace firmware::target {
namespace {

firmware::application::RuntimeCounterService* active_service = nullptr;
constexpr std::uint32_t persistence_interval_milliseconds = 1000U;
constexpr std::uint32_t runtime_task_stack_size = 4096U;
constexpr UBaseType_t runtime_task_priority = 3U;

void runtime_counter_task(void*) {
    NvsRuntimeCounterAdapter nvs;
    firmware::application::RuntimeCounterService service(nvs);
    active_service = &service;
    const std::uint64_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
    service.initialize(start);
    // Capture the first-boot wall-clock value once the persistent service is ready.
    service.record_first_boot(static_cast<std::int64_t>(std::time(nullptr)));
    for (;;) {
        const std::uint64_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        service.save_power_on(now);
        vTaskDelay(pdMS_TO_TICKS(persistence_interval_milliseconds));
    }
}

}  // namespace

void record_runtime_first_boot(std::int64_t unix_seconds) {
    if (active_service != nullptr) {
        active_service->record_first_boot(unix_seconds);
    }
}

void notify_runtime_play_state(bool running, std::uint64_t monotonic_milliseconds) {
    if (active_service != nullptr) {
        active_service->play_running_changed(running, monotonic_milliseconds);
    }
}

void request_runtime_persistence(std::uint64_t monotonic_milliseconds) {
    if (active_service != nullptr) {
        active_service->save_power_on(monotonic_milliseconds);
    }
}

void RuntimeCounterTask::start() {
    xTaskCreate(runtime_counter_task, "runtime_counters",
                runtime_task_stack_size, nullptr, runtime_task_priority, nullptr);
}

}  // namespace firmware::target
