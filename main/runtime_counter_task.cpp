// Implements periodic runtime-counter persistence over FreeRTOS timing.
#include "runtime_counter_task.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "firmware/application/runtime_counters.hpp"
#include "nvs_runtime_counter_adapter.hpp"

#include <cstdint>

namespace firmware::target {
namespace {

void runtime_counter_task(void*) {
    NvsRuntimeCounterAdapter nvs;
    firmware::application::RuntimeCounterService service(nvs);
    const std::uint64_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
    service.initialize(start);
    for (;;) {
        const std::uint64_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        service.save_power_on(now);
        vTaskDelay(pdMS_TO_TICKS(1000U));
    }
}

}  // namespace

void RuntimeCounterTask::start() {
    xTaskCreate(runtime_counter_task, "runtime_counters", 4096U, nullptr, 3U,
                nullptr);
}

}  // namespace firmware::target
