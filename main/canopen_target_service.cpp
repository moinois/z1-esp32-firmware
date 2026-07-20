// Runs the portable CANopen service through ESP-IDF TWAI and FreeRTOS adapters.
#include "canopen_target_service.hpp"

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstddef>
#include <cstdint>

namespace firmware::target {
namespace {

constexpr char task_name[] = "canopen";
constexpr std::uint32_t task_stack_size = 4096U;
constexpr UBaseType_t task_priority = 8U;
constexpr std::uint32_t cycles_per_output_sample =
    application::can_output_monitor::sample_period_milliseconds /
    core::canopen::processing_period_milliseconds;

}  // namespace

CanopenTargetService::CanopenTargetService()
    : service_(*this), output_monitor_(service_.dictionary(), *this) {}

bool CanopenTargetService::start() {
    if (!adapter_.initialize()) {
        return false;
    }
    output_monitor_.start();
    if (xTaskCreate(task_entry,
                    task_name,
                    task_stack_size,
                    this,
                    task_priority,
                    nullptr) == pdPASS) {
        return true;
    }
    adapter_.shutdown();
    return false;
}

void CanopenTargetService::transmit(const core::CanFrame& frame) {
    adapter_.transmit(frame);
}

void CanopenTargetService::restart_mainboard() {
    esp_restart();
}

void CanopenTargetService::log_info(std::string_view tag,
                                    std::string_view message) {
    ESP_LOGI(tag.data(), "%.*s", static_cast<int>(message.size()),
             message.data());
}

void CanopenTargetService::task_entry(void* context) {
    static_cast<CanopenTargetService*>(context)->run();
}

void CanopenTargetService::run() {
    TickType_t last_wake = xTaskGetTickCount();
    std::uint32_t completed_cycles = 0U;
    for (;;) {
        core::CanFrame frame;
        while (adapter_.receive(frame)) {
            service_.receive(frame);
        }
        service_.process_cycle();

        vTaskDelayUntil(
            &last_wake,
            pdMS_TO_TICKS(core::canopen::processing_period_milliseconds));
        ++completed_cycles;
        if (completed_cycles == cycles_per_output_sample) {
            output_monitor_.sample();
            completed_cycles = 0U;
        }
    }
}

}  // namespace firmware::target
