/** @file @brief Implements an opt-in bounded binary UART trace on removable storage. */
#include "controller_uart_trace.hpp"

#include "sd_access_diagnostics.hpp"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

namespace firmware::target {
namespace {

constexpr char enable_path[] = "/sd/uart-trace.enable";
constexpr char trace_path[] = "/sd/uart-trace.bin";
constexpr std::array<std::uint8_t, 4U> record_magic{'Z', '1', 'U', 'T'};
constexpr std::uint8_t record_version = 1U;
constexpr std::size_t maximum_payload_size = 1024U;
constexpr std::size_t queue_capacity = 16U;
constexpr std::uint64_t maximum_trace_size = 4U * 1024U * 1024U;
constexpr std::uint32_t trace_task_stack_size = 4096U;
constexpr UBaseType_t trace_task_priority = 2U;

struct TraceChunk {
    std::uint64_t timestamp_microseconds;
    std::uint32_t sequence;
    std::uint16_t size;
    std::uint8_t direction;
    std::array<std::uint8_t, maximum_payload_size> payload;
};

QueueHandle_t trace_queue = nullptr;
SemaphoreHandle_t trace_file_mutex = nullptr;
FILE* trace_file = nullptr;
std::uint64_t trace_size = 0U;
std::atomic<bool> trace_enabled{false};
std::atomic<std::uint32_t> next_sequence{0U};

void close_trace_locked() {
    FILE* const file = trace_file;
    trace_file = nullptr;
    trace_size = 0U;
    trace_enabled.store(false);
    if (file != nullptr) {
        static_cast<void>(std::fflush(file));
        static_cast<void>(std::fclose(file));
    }
}

bool sentinel_exists() {
    struct stat status {};
    return stat(enable_path, &status) == 0 && S_ISREG(status.st_mode);
}

bool open_trace() {
    if (trace_file != nullptr) return true;
    FILE* file = std::fopen(trace_path, "ab");
    if (file == nullptr) return false;
    static_cast<void>(std::setvbuf(file, nullptr, _IOFBF, 4096U));
    if (std::fseek(file, 0L, SEEK_END) != 0) {
        static_cast<void>(std::fclose(file));
        return false;
    }
    const long position = std::ftell(file);
    if (position < 0L ||
        static_cast<std::uint64_t>(position) >= maximum_trace_size) {
        static_cast<void>(std::fclose(file));
        return false;
    }
    trace_file = file;
    trace_size = static_cast<std::uint64_t>(position);
    return true;
}

void append_u16_le(std::array<std::uint8_t, 20U>& header,
                   std::size_t offset, std::uint16_t value) {
    header[offset] = static_cast<std::uint8_t>(value & 0xffU);
    header[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
}

void append_u64_le(std::array<std::uint8_t, 20U>& header,
                   std::size_t offset, std::uint64_t value) {
    for (std::size_t index = 0U; index < 8U; ++index) {
        header[offset + index] = static_cast<std::uint8_t>(value >> (index * 8U));
    }
}

void append_u32_le(std::array<std::uint8_t, 20U>& header,
                   std::size_t offset, std::uint32_t value) {
    for (std::size_t index = 0U; index < 4U; ++index) {
        header[offset + index] = static_cast<std::uint8_t>(value >> (index * 8U));
    }
}

void write_chunk(const TraceChunk& chunk) {
    if (xSemaphoreTake(trace_file_mutex, portMAX_DELAY) != pdTRUE) return;
    if (!sd_storage_mounted()) {
        close_trace_locked();
        xSemaphoreGive(trace_file_mutex);
        return;
    }
    if (!open_trace()) {
        trace_enabled.store(false);
        xSemaphoreGive(trace_file_mutex);
        return;
    }

    constexpr std::size_t header_size = 20U;
    const std::uint64_t record_size = header_size + chunk.size;
    if (trace_size > maximum_trace_size ||
        record_size > maximum_trace_size - trace_size) {
        close_trace_locked();
        xSemaphoreGive(trace_file_mutex);
        return;
    }

    std::array<std::uint8_t, header_size> header{};
    std::copy(record_magic.begin(), record_magic.end(), header.begin());
    header[4U] = record_version;
    header[5U] = chunk.direction;
    append_u16_le(header, 6U, chunk.size);
    append_u64_le(header, 8U, chunk.timestamp_microseconds);
    append_u32_le(header, 16U, chunk.sequence);
    if (std::fwrite(header.data(), 1U, header.size(), trace_file) != header.size() ||
        std::fwrite(chunk.payload.data(), 1U, chunk.size, trace_file) != chunk.size) {
        close_trace_locked();
        xSemaphoreGive(trace_file_mutex);
        return;
    }
    trace_size += record_size;
    static_cast<void>(std::fflush(trace_file));
    xSemaphoreGive(trace_file_mutex);
}

void trace_task(void*) {
    TraceChunk chunk{};
    TickType_t last_sentinel_check = 0U;
    while (true) {
        const TickType_t now = xTaskGetTickCount();
        if (now - last_sentinel_check >= pdMS_TO_TICKS(1000U)) {
            last_sentinel_check = now;
            if (xSemaphoreTake(trace_file_mutex, portMAX_DELAY) == pdTRUE) {
                if (!sd_storage_mounted() || !sentinel_exists()) {
                    close_trace_locked();
                } else {
                    trace_enabled.store(open_trace());
                }
                xSemaphoreGive(trace_file_mutex);
            }
        }
        if (!trace_enabled.load()) {
            vTaskDelay(pdMS_TO_TICKS(1000U));
            continue;
        }
        if (xQueueReceive(trace_queue, &chunk, pdMS_TO_TICKS(250U)) == pdTRUE) {
            write_chunk(chunk);
        }
    }
}

}  // namespace

void start_controller_uart_trace() {
    if (trace_queue != nullptr) return;
    trace_queue = xQueueCreate(queue_capacity, sizeof(TraceChunk));
    if (trace_queue == nullptr) return;
    trace_file_mutex = xSemaphoreCreateMutex();
    if (trace_file_mutex == nullptr) {
        vQueueDelete(trace_queue);
        trace_queue = nullptr;
        return;
    }
    if (xTaskCreate(trace_task, "uart_trace", trace_task_stack_size, nullptr,
                    trace_task_priority, nullptr) != pdPASS) {
        vSemaphoreDelete(trace_file_mutex);
        trace_file_mutex = nullptr;
        vQueueDelete(trace_queue);
        trace_queue = nullptr;
    }
}

void trace_controller_uart(ControllerUartTraceDirection direction,
                           core::BytesView bytes) {
    if (trace_queue == nullptr || !trace_enabled.load() ||
        bytes.size() == 0U) {
        return;
    }
    TraceChunk chunk{};
    chunk.timestamp_microseconds = static_cast<std::uint64_t>(esp_timer_get_time());
    chunk.sequence = next_sequence.fetch_add(1U);
    chunk.size = static_cast<std::uint16_t>(
        std::min(bytes.size(), maximum_payload_size));
    chunk.direction = static_cast<std::uint8_t>(direction);
    std::memcpy(chunk.payload.data(), bytes.data(), chunk.size);
    static_cast<void>(xQueueSend(trace_queue, &chunk, 0U));
}

void controller_uart_trace_storage_unmounted() {
    if (trace_file_mutex == nullptr) return;
    if (xSemaphoreTake(trace_file_mutex, portMAX_DELAY) == pdTRUE) {
        close_trace_locked();
        xSemaphoreGive(trace_file_mutex);
    }
}

}  // namespace firmware::target
