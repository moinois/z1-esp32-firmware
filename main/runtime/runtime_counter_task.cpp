/** @file @brief Implements periodic runtime-counter persistence over FreeRTOS timing. */
#include "runtime_counter_task.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "application/runtime/runtime_counters.hpp"
#include "application/diagnostics/runtime_diagnostics.hpp"
#include "nvs_runtime_counter_adapter.hpp"
#include "esp_log.h"

#include <ctime>
#include <cstdint>

namespace firmware::target {
namespace {

constexpr std::uint32_t persistence_interval_milliseconds = 1000U;
constexpr std::uint32_t runtime_task_stack_size = 4096U;
constexpr UBaseType_t runtime_task_priority = 3U;
constexpr UBaseType_t runtime_event_capacity = 32U;

enum class RuntimeEventKind : std::uint8_t {
    first_boot,
    play_state,
    persist,
};

struct RuntimeEvent {
    RuntimeEventKind kind;
    std::int64_t value;
    std::uint64_t milliseconds;
};

QueueHandle_t runtime_events = nullptr;

void warn_queue_full(firmware::application::RuntimeDiagnosticEvent event) {
    const auto message = firmware::application::runtime_diagnostic(event);
    ESP_LOGW("APP_NVS", "%s", message.c_str());
}

bool enqueue_runtime_event(const RuntimeEvent& event,
                           firmware::application::RuntimeDiagnosticEvent failure) {
    if (runtime_events != nullptr &&
        xQueueSend(runtime_events, &event, 0U) == pdTRUE) {
        return true;
    }
    warn_queue_full(failure);
    return false;
}

void runtime_counter_task(void*) {
    NvsRuntimeCounterAdapter nvs;
    firmware::application::RuntimeCounterService service(nvs);
    const std::uint64_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
    service.initialize(start);
    // Capture the first-boot wall-clock value once the persistent service is ready.
    service.record_first_boot(static_cast<std::int64_t>(std::time(nullptr)));
    std::uint64_t next_persistence = start;
    for (;;) {
        const std::uint64_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now >= next_persistence) {
            service.save_power_on(now);
            next_persistence = now + persistence_interval_milliseconds;
        }
        const std::uint64_t remaining = next_persistence > now
                                            ? next_persistence - now
                                            : 0U;
        RuntimeEvent event{};
        if (xQueueReceive(runtime_events, &event,
                          pdMS_TO_TICKS(remaining)) != pdTRUE) {
            continue;
        }
        switch (event.kind) {
            case RuntimeEventKind::first_boot:
                service.record_first_boot(event.value);
                break;
            case RuntimeEventKind::play_state:
                service.play_running_changed(event.value != 0,
                                             event.milliseconds);
                break;
            case RuntimeEventKind::persist:
                service.save_power_on(event.milliseconds);
                break;
        }
    }
}

}  // namespace

void record_runtime_first_boot(std::int64_t unix_seconds) {
    static_cast<void>(enqueue_runtime_event(
        {RuntimeEventKind::first_boot, unix_seconds, 0U},
        firmware::application::RuntimeDiagnosticEvent::first_boot_queue_full));
}

void notify_runtime_play_state(bool running, std::uint64_t monotonic_milliseconds) {
    static_cast<void>(enqueue_runtime_event(
        {RuntimeEventKind::play_state, running ? 1 : 0,
         monotonic_milliseconds},
        firmware::application::RuntimeDiagnosticEvent::play_state_queue_full));
}

void request_runtime_persistence(std::uint64_t monotonic_milliseconds) {
    static_cast<void>(enqueue_runtime_event(
        {RuntimeEventKind::persist, 0, monotonic_milliseconds},
        firmware::application::RuntimeDiagnosticEvent::persistence_queue_full));
}

void RuntimeCounterTask::start() {
    runtime_events = xQueueCreate(runtime_event_capacity, sizeof(RuntimeEvent));
    if (runtime_events == nullptr) return;
    if (xTaskCreate(runtime_counter_task, "runtime_counters",
                    runtime_task_stack_size, nullptr, runtime_task_priority,
                    nullptr) != pdPASS) {
        vQueueDelete(runtime_events);
        runtime_events = nullptr;
    }
}

}  // namespace firmware::target
