/** @file @brief Runs the portable CANopen service through ESP-IDF TWAI and FreeRTOS adapters. */
#include "canopen_target_service.hpp"

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

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
    : service_(*this), output_monitor_(service_.dictionary(), *this) {
    sdo_mutex_ = xSemaphoreCreateMutex();
    sdo_response_ = xSemaphoreCreateBinary();
}
CanopenTargetService* active_service = nullptr;

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
        active_service = this;
        return true;
    }
    adapter_.shutdown();
    return false;
}

CanopenTargetService* active_canopen_target_service() {
    return active_service;
}

void CanopenTargetService::transmit(const core::CanFrame& frame) {
    adapter_.transmit(frame);
}

void CanopenTargetService::restart_mainboard() {
    ESP_LOGW("CANOPEN", "Mainboard restart requested by CANopen service");
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
            if (frame.identifier == core::canopen::nmt_identifier &&
                frame.size >= 2U && frame.data[0] == 0x81U) {
                ESP_LOGW("CANOPEN", "NMT reset-node received target=%u",
                         static_cast<unsigned>(frame.data[1]));
            }
            if (sdo_mailbox_.pending()) {
                const auto response = sdo_mailbox_.accept(frame);
                if (response.has_value()) {
                    sdo_result_ = response;
                    xSemaphoreGive(sdo_response_);
                    continue;
                }
            }
            service_.receive(frame);
        }
        service_.set_error_register(adapter_.error_register());
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

std::optional<std::uint32_t> CanopenTargetService::read_remote_u32(
    std::uint8_t node, std::uint16_t index, std::uint8_t subindex,
    std::uint32_t timeout_milliseconds) {
    if (sdo_mutex_ == nullptr || sdo_response_ == nullptr ||
        xSemaphoreTake(sdo_mutex_, pdMS_TO_TICKS(timeout_milliseconds)) != pdTRUE) {
        return std::nullopt;
    }
    while (xSemaphoreTake(sdo_response_, 0U) == pdTRUE) {}
    sdo_result_.reset();
    const auto request = sdo_mailbox_.begin_upload(
        node, index, subindex,
        static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL),
        timeout_milliseconds);
    if (!request.has_value() || !adapter_.transmit(*request)) {
        xSemaphoreGive(sdo_mutex_);
        return std::nullopt;
    }
    const bool received = xSemaphoreTake(
        sdo_response_, pdMS_TO_TICKS(timeout_milliseconds)) == pdTRUE;
    const auto result = received && sdo_result_.has_value() &&
                                !sdo_result_->aborted
                            ? std::optional<std::uint32_t>(sdo_result_->value)
                            : std::nullopt;
    if (!received) {
        sdo_mailbox_.timed_out(
            static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL));
    }
    xSemaphoreGive(sdo_mutex_);
    return result;
}

bool CanopenTargetService::write_remote_u32(
    std::uint8_t node, std::uint16_t index, std::uint8_t subindex,
    std::uint32_t value, std::uint32_t timeout_milliseconds) {
    if (sdo_mutex_ == nullptr || sdo_response_ == nullptr ||
        xSemaphoreTake(sdo_mutex_, pdMS_TO_TICKS(timeout_milliseconds)) != pdTRUE) {
        return false;
    }
    while (xSemaphoreTake(sdo_response_, 0U) == pdTRUE) {}
    sdo_result_.reset();
    const auto request = sdo_mailbox_.begin_download(
        node, index, subindex, value,
        static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL),
        timeout_milliseconds);
    if (!request.has_value() || !adapter_.transmit(*request)) {
        xSemaphoreGive(sdo_mutex_);
        return false;
    }
    const bool received = xSemaphoreTake(
        sdo_response_, pdMS_TO_TICKS(timeout_milliseconds)) == pdTRUE;
    const bool succeeded = received && sdo_result_.has_value() &&
                           !sdo_result_->aborted;
    if (!received) {
        sdo_mailbox_.timed_out(
            static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL));
    }
    xSemaphoreGive(sdo_mutex_);
    return succeeded;
}

}  // namespace firmware::target
