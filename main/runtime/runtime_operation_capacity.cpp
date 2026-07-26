// Implements the shared eight-slot FreeRTOS operation capacity.
#include "runtime_operation_capacity.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace firmware::target {
namespace {

constexpr UBaseType_t operation_capacity = 8U;
SemaphoreHandle_t operation_slots = nullptr;

SemaphoreHandle_t slots() {
    if (operation_slots == nullptr) {
        operation_slots = xSemaphoreCreateCounting(operation_capacity,
                                                   operation_capacity);
    }
    return operation_slots;
}

}  // namespace

bool admit_runtime_operation(std::uint32_t wait_milliseconds) {
    const auto semaphore = slots();
    return semaphore != nullptr &&
           xSemaphoreTake(semaphore, pdMS_TO_TICKS(wait_milliseconds)) == pdTRUE;
}

void complete_runtime_operation() {
    const auto semaphore = slots();
    if (semaphore != nullptr) {
        static_cast<void>(xSemaphoreGive(semaphore));
    }
}

}  // namespace firmware::target
