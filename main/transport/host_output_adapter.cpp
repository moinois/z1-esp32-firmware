/** @file @brief Delivers globally selected host output to transport endpoints. */
#include "host_output_adapter.hpp"

#include "tcp_control_adapter.hpp"
#include "usb_device_adapter.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <atomic>
#include <chrono>

namespace firmware::target {
namespace {

constexpr std::uint32_t output_task_stack_size = 4096U;
constexpr UBaseType_t output_task_priority = 4U;
constexpr TickType_t selection_interval = pdMS_TO_TICKS(10U);

firmware::application::HostOutputScheduler scheduler;
std::atomic_bool usb_active{false};
std::atomic_bool tcp_active{false};
std::atomic_bool task_started{false};

void update_destinations() {
    scheduler.set_active_destinations(
        usb_active.load(std::memory_order_acquire),
        tcp_active.load(std::memory_order_acquire));
}

void deliver(const firmware::application::PendingHostOutput& output) {
    if (!output.destination.host.has_value()) {
        // TRN-001 fixes broadcast expansion order independently of which
        // destination ultimately accepts the selected frame.
        static_cast<void>(deliver_usb_frame(output.frame));
        deliver_broadcast_tcp_frame(output.frame);
        return;
    }
    const auto& host = *output.destination.host;
    if (host.transport == firmware::application::HostTransport::usb) {
        static_cast<void>(deliver_usb_frame(output.frame));
    } else {
        static_cast<void>(deliver_tcp_frame(host, output.frame));
    }
}

void output_task(void*) {
    for (;;) {
        auto selection = scheduler.select();
        if (selection.download_data.has_value()) {
            deliver(*selection.download_data);
        }
        for (const auto& output : selection.non_download) deliver(output);
        if (selection.delay_before_next_selection) {
            vTaskDelay(selection_interval);
        } else {
            taskYIELD();
        }
    }
}

bool admitted(firmware::application::HostOutputAdmission result) {
    return result == firmware::application::HostOutputAdmission::accepted;
}

}  // namespace

bool initialize_host_output_adapter() {
    bool expected = false;
    if (!task_started.compare_exchange_strong(expected, true,
                                               std::memory_order_acq_rel)) {
        return true;
    }
    if (xTaskCreate(output_task, "host_output", output_task_stack_size, nullptr,
                    output_task_priority, nullptr) != pdPASS) {
        task_started.store(false, std::memory_order_release);
        return false;
    }
    return true;
}

bool queue_host_frame(const firmware::core::Frame& frame,
                      firmware::application::HostIdentity destination,
                      firmware::application::HostOutputSource source) {
    return admitted(scheduler.admit(
        frame,
        firmware::application::HostOutputDestination::addressed(destination),
        source));
}

bool queue_host_listing(const firmware::core::Frame& frame,
                        firmware::application::HostIdentity destination) {
    return admitted(scheduler.admit_listing(
        frame,
        firmware::application::HostOutputDestination::addressed(destination),
        std::chrono::milliseconds(300)));
}

bool broadcast_host_frame(const firmware::core::Frame& frame,
                          firmware::application::HostOutputSource source) {
    return admitted(scheduler.admit(
        frame, firmware::application::HostOutputDestination::broadcast(), source));
}

void set_host_output_usb_active(bool active) {
    usb_active.store(active, std::memory_order_release);
    update_destinations();
}

void set_host_output_tcp_active(bool active) {
    tcp_active.store(active, std::memory_order_release);
    update_destinations();
}

}  // namespace firmware::target
