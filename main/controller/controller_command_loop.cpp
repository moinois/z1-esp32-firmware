/** @file @brief Implements UART frame decoding and wall-clock command dispatch. */
#include "controller_command_loop.hpp"

#include "controller_channel_adapter.hpp"
#include "hardware_adapter_factory.hpp"
#include "controller_transfer_adapter.hpp"
#include "controller_play_adapter.hpp"
#include "play_runtime_state.hpp"
#include "runtime_status_adapter.hpp"
#include "runtime_play_observer.hpp"
#include "wall_clock_adapter.hpp"
#include "wall_clock_command_dispatcher.hpp"
#include "serial_number_adapter.hpp"
#include "controller_runtime_command_adapter.hpp"
#include "firmware_update_adapter.hpp"
#include "application/runtime/serial_number.hpp"
#include "application/web/recording_commands.hpp"
#include "recording_request_state.hpp"
#include "core/protocol/text.hpp"
#include "core/protocol/frame.hpp"

#include <string_view>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "application/controller/controller_frame_forwarder.hpp"
#include "application/controller/controller_transfer.hpp"
#include "application/diagnostics/controller_diagnostics.hpp"
#include "application/controller/controller_query.hpp"
#include "application/controller/controller_link.hpp"
#include "application/controller/controller_firmware_transfer.hpp"
#include "application/controller/controller_config_transfer.hpp"
#include "application/controller/controller_factory_transfer.hpp"
#include "application/runtime/local_command_queue.hpp"
#include "application/runtime/router.hpp"
#include "application/playback/play_controller.hpp"
#include "core/protocol/protocol_constants.hpp"
#include "tcp_control_adapter.hpp"
#include "host_output_adapter.hpp"

#include <cstdint>
#include <optional>

namespace firmware::target {
namespace {

constexpr char controller_uart_tag[] = "uart_task";
constexpr std::size_t controller_read_buffer_size = 256U;
constexpr std::uint32_t controller_task_stack_size = 6144U;
constexpr std::uint32_t controller_consumer_stack_size = 4096U;
constexpr UBaseType_t controller_task_priority = 5U;
constexpr TickType_t controller_consumer_poll_ticks = pdMS_TO_TICKS(1U);

// Writes one complete frame and emits the specified diagnostic on failure.
void write_controller_frame(ControllerChannelAdapter& channel,
                            firmware::core::BytesView frame) {
    const int written = channel.write(frame);
    if (written != static_cast<int>(frame.size())) {
        ESP_LOGE(controller_uart_tag, "UART send failed");
    }
}

firmware::application::ControllerFrameForwarder controller_forwarder;
SemaphoreHandle_t controller_forwarder_mutex = nullptr;
firmware::application::ControllerFirmwareTransfer* active_firmware = nullptr;
firmware::application::ControllerConfigTransfer* active_configuration = nullptr;
firmware::application::ControllerFactoryTransfer* active_factory = nullptr;
firmware::application::ControllerTransferInbox firmware_inbox(
    firmware::core::protocol::firmware_family);
firmware::application::ControllerTransferInbox configuration_inbox(
    firmware::core::protocol::configuration_family);
firmware::application::ControllerTransferInbox factory_inbox(
    firmware::core::protocol::factory_family);
firmware::application::ControllerTransferInbox play_inbox(
    firmware::core::protocol::play_family);
SemaphoreHandle_t firmware_inbox_mutex = nullptr;
SemaphoreHandle_t configuration_inbox_mutex = nullptr;
SemaphoreHandle_t factory_inbox_mutex = nullptr;
SemaphoreHandle_t play_inbox_mutex = nullptr;

bool enqueue_controller_inbox(
    firmware::application::ControllerTransferInbox& inbox,
    SemaphoreHandle_t mutex, const firmware::core::Frame& frame) {
    if (mutex == nullptr || xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    const std::size_t pending = inbox.pending();
    const bool queued = inbox.enqueue(frame);
    if (!queued && pending >= 32U) {
        const auto message = firmware::application::
            controller_receive_queue_full_diagnostic(
                frame.type, static_cast<std::uint64_t>(esp_timer_get_time()),
                pending);
        ESP_LOGW(controller_uart_tag, "%s", message.c_str());
    }
    xSemaphoreGive(mutex);
    return queued;
}

std::optional<firmware::core::Frame> take_controller_inbox(
    firmware::application::ControllerTransferInbox& inbox,
    SemaphoreHandle_t mutex) {
    if (mutex == nullptr || xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return std::nullopt;
    }
    auto frame = inbox.take_ready(
        static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL));
    xSemaphoreGive(mutex);
    return frame;
}

void firmware_transfer_task(void*) {
    ControllerTransferAdapter port(HardwareAdapterFactory::controller_channel());
    static firmware::application::ControllerFirmwareTransfer transfer;
    active_firmware = &transfer;
    for (;;) {
        if (auto frame = take_controller_inbox(firmware_inbox,
                                               firmware_inbox_mutex)) {
            transfer.handle(*frame,
                            static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL),
                            port);
        }
        vTaskDelay(controller_consumer_poll_ticks);
    }
}

void configuration_transfer_task(void*) {
    ControllerTransferAdapter port(HardwareAdapterFactory::controller_channel());
    static firmware::application::ControllerConfigTransfer transfer;
    active_configuration = &transfer;
    for (;;) {
        if (auto frame = take_controller_inbox(configuration_inbox,
                                               configuration_inbox_mutex)) {
            transfer.handle(*frame, port);
        }
        vTaskDelay(controller_consumer_poll_ticks);
    }
}

void factory_transfer_task(void*) {
    ControllerTransferAdapter port(HardwareAdapterFactory::controller_channel());
    static firmware::application::ControllerFactoryTransfer transfer;
    active_factory = &transfer;
    for (;;) {
        if (auto frame = take_controller_inbox(factory_inbox,
                                               factory_inbox_mutex)) {
            transfer.handle(*frame, port);
        }
        vTaskDelay(controller_consumer_poll_ticks);
    }
}

void play_transfer_task(void*) {
    auto& play_session = shared_play_session();
    firmware::application::PlayController controller(play_session);
    ControllerPlayAdapter port(HardwareAdapterFactory::controller_channel());
    for (;;) {
        if (auto frame = take_controller_inbox(play_inbox, play_inbox_mutex)) {
            port.diagnose(firmware::application::playback_dequeue_diagnostic(
                static_cast<std::uint64_t>(esp_timer_get_time()), frame->type,
                frame->payload));
            controller.handle(
                *frame,
                static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL), port);
        }
        vTaskDelay(controller_consumer_poll_ticks);
    }
}

bool enqueue_controller_frame_impl(const firmware::core::Frame& frame,
                                   bool diagnose_capacity) {
    if (controller_forwarder_mutex == nullptr) {
        return false;
    }
    if (xSemaphoreTake(controller_forwarder_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    const bool capacity_full = controller_forwarder.full();
    const bool queued = controller_forwarder.forward(frame);
    if (!queued && capacity_full && diagnose_capacity) {
        const std::string message =
            firmware::application::controller_queue_full_diagnostic(frame.type);
        ESP_LOGW("APP_ROUTER", "%s", message.c_str());
    }
    xSemaphoreGive(controller_forwarder_mutex);
    return queued;
}

void drain_forwarded_frames(ControllerChannelAdapter& channel) {
    for (;;) {
        std::optional<firmware::core::ByteVector> item;
        if (xSemaphoreTake(controller_forwarder_mutex, portMAX_DELAY) == pdTRUE) {
            item = controller_forwarder.take_ready(
                static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL));
            xSemaphoreGive(controller_forwarder_mutex);
        }
        if (!item.has_value()) {
            return;
        }
        write_controller_frame(channel, *item);
    }
}

void controller_command_task(void*) {
    ControllerChannelAdapter& channel =
        HardwareAdapterFactory::controller_channel();
    if (!channel.initialize()) {
        vTaskDelete(nullptr);
        return;
    }
    EspWallClockAdapter wall_clock(&channel);
    WallClockCommandDispatcher dispatcher(wall_clock);
    NvsSerialNumberAdapter serial_port(&channel);
    firmware::application::SerialNumberService serial_service(serial_port);
    ControllerRuntimeCommandAdapter runtime_port(channel);
    firmware::application::RuntimeCommandService runtime_service(runtime_port);
    RecordingRequestState recording_state;
    firmware::core::StreamDecoder decoder(
        firmware::core::StreamPolicy::controller_uart());
    firmware::application::LocalCommandQueue local_commands;
    firmware::application::ControllerQueryScheduler query_scheduler(
        static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL));
    firmware::application::ControllerActivityMonitor activity_monitor(
        static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL));
    std::uint8_t input[controller_read_buffer_size];
    for (;;) {
        drain_forwarded_frames(channel);
        const auto due_queries = query_scheduler.poll(
            static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL), true);
        for (const auto& query : due_queries) {
            const auto encoded = firmware::core::encode_controller_frame(query);
            if (!encoded.empty()) {
                write_controller_frame(channel, encoded);
            }
        }
        const auto now_milliseconds =
            static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL);
        for (const auto& alarm : activity_monitor.poll(now_milliseconds)) {
            static_cast<void>(broadcast_host_frame(
                alarm,
                firmware::application::HostOutputSource::inactivity_alarm));
        }
        const int count = channel.read(input, sizeof(input));
        if (count <= 0) continue;
        const auto frames = decoder.push(
            {input, static_cast<std::size_t>(count)});
        for (const auto& frame : frames) {
            activity_monitor.record_valid_frame(
                static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL));
            const auto route = shared_host_router().from_controller(frame);
            if (route.has(firmware::application::RouteTarget::broadcast)) {
                const auto admission = admit_host_broadcast(
                    frame,
                    firmware::application::HostOutputSource::motion_board_unchanged);
                if (admission == firmware::application::
                                     HostOutputAdmission::purged_at_capacity) {
                    ESP_LOGW(controller_uart_tag, "%s",
                             firmware::application::
                                 controller_host_output_purge_diagnostic()
                                     .data());
                }
            }
            if (frame.type == firmware::core::protocol::machine_status) {
                shared_controller_snapshots().update_status(frame.payload);
                set_controller_running(firmware::core::status_reports_running(
                    std::string_view(reinterpret_cast<const char*>(
                                         frame.payload.data()),
                                     frame.payload.size())));
            } else if (frame.type == firmware::core::protocol::diagnostic_data) {
                shared_controller_snapshots().update_diagnostic(frame.payload);
            } else if (frame.type == firmware::core::protocol::controller_version) {
                shared_controller_snapshots().update_version(frame.payload);
            }
            const std::uint8_t family =
                frame.type & firmware::core::protocol::family_mask;
            if (family == firmware::core::protocol::firmware_family) {
                static_cast<void>(enqueue_controller_inbox(
                    firmware_inbox, firmware_inbox_mutex, frame));
                continue;
            }
            if (family == firmware::core::protocol::configuration_family) {
                static_cast<void>(enqueue_controller_inbox(
                    configuration_inbox, configuration_inbox_mutex, frame));
                continue;
            }
            if (family == firmware::core::protocol::factory_family) {
                static_cast<void>(enqueue_controller_inbox(
                    factory_inbox, factory_inbox_mutex, frame));
                continue;
            }
            if (family == firmware::core::protocol::play_family) {
                static_cast<void>(enqueue_controller_inbox(
                    play_inbox, play_inbox_mutex, frame));
                continue;
            }
            dispatcher.dispatch(frame);
            if (frame.type != firmware::core::protocol::general_command) continue;
            const auto match = firmware::core::recognize_command(frame.payload);
            if (match.kind == firmware::core::CommandKind::serial_get ||
                match.kind == firmware::core::CommandKind::serial_set ||
                match.kind == firmware::core::CommandKind::clear_first_time ||
                match.kind == firmware::core::CommandKind::upgrade ||
                match.kind == firmware::core::CommandKind::reset ||
                match.kind == firmware::core::CommandKind::record_start ||
                match.kind == firmware::core::CommandKind::record_stop) {
                static_cast<void>(local_commands.enqueue(frame));
            }
        }
        if (const auto local_frame = local_commands.dequeue();
            local_frame.has_value()) {
            const std::string_view command(
                reinterpret_cast<const char*>(local_frame->payload.data()),
                local_frame->payload.size());
            const auto match = firmware::core::recognize_command(
                local_frame->payload);
            if (match.kind == firmware::core::CommandKind::serial_get) {
                serial_service.handle_get(command);
            } else if (match.kind == firmware::core::CommandKind::serial_set) {
                serial_service.handle_set(command);
            } else if (match.kind == firmware::core::CommandKind::clear_first_time) {
                runtime_service.handle_clear_first_boot(command);
            } else if (match.kind == firmware::core::CommandKind::upgrade ||
                       match.kind == firmware::core::CommandKind::reset) {
                request_firmware_update_processing();
            } else {
                const auto result = firmware::application::handle_recording_command(
                    match.kind, recording_state.requested());
                recording_state.set_requested(result.requested);
                const auto encoded =
                    firmware::core::encode_controller_frame(result.response);
                if (!encoded.empty()) {
                    write_controller_frame(channel, encoded);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10U));
        }
    }
}

}  // namespace

void ControllerCommandLoop::start() {
    if (controller_forwarder_mutex == nullptr) {
        controller_forwarder_mutex = xSemaphoreCreateMutex();
    }
    // BOOT-013 makes controller transmit and every receive consumer
    // independent startup attempts. A missing forwarder mutex disables output
    // admission, but must not prevent UART receive or family retention.
    firmware_inbox_mutex = xSemaphoreCreateMutex();
    configuration_inbox_mutex = xSemaphoreCreateMutex();
    factory_inbox_mutex = xSemaphoreCreateMutex();
    play_inbox_mutex = xSemaphoreCreateMutex();
    if (firmware_inbox_mutex != nullptr) {
        static_cast<void>(xTaskCreate(
            firmware_transfer_task, "controller_fw", controller_consumer_stack_size,
            nullptr, controller_task_priority, nullptr));
    }
    if (configuration_inbox_mutex != nullptr) {
        static_cast<void>(xTaskCreate(
            configuration_transfer_task, "controller_cfg",
            controller_consumer_stack_size, nullptr, controller_task_priority,
            nullptr));
    }
    if (factory_inbox_mutex != nullptr) {
        static_cast<void>(xTaskCreate(
            factory_transfer_task, "controller_fac", controller_consumer_stack_size,
            nullptr, controller_task_priority, nullptr));
    }
    if (play_inbox_mutex != nullptr) {
        static_cast<void>(xTaskCreate(
            play_transfer_task, "controller_play", controller_consumer_stack_size,
            nullptr, controller_task_priority, nullptr));
    }
    static_cast<void>(xTaskCreate(
        controller_command_task, "controller_commands", controller_task_stack_size,
        nullptr, controller_task_priority, nullptr));
}

bool enqueue_controller_frame(const firmware::core::Frame& frame) {
    return enqueue_controller_frame_impl(frame, false);
}

bool enqueue_generated_controller_frame(const firmware::core::Frame& frame) {
    return enqueue_controller_frame_impl(frame, true);
}

PlayControllerEnqueueResult enqueue_play_controller_frame(
    const firmware::core::Frame& frame) {
    if (controller_forwarder_mutex == nullptr ||
        xSemaphoreTake(controller_forwarder_mutex, portMAX_DELAY) != pdTRUE) {
        return PlayControllerEnqueueResult::unavailable;
    }
    const bool capacity_full = controller_forwarder.full();
    const bool queued = controller_forwarder.forward(frame);
    xSemaphoreGive(controller_forwarder_mutex);
    if (queued) return PlayControllerEnqueueResult::accepted;
    return capacity_full ? PlayControllerEnqueueResult::capacity_full
                         : PlayControllerEnqueueResult::unavailable;
}

bool controller_firmware_transfer_active() {
    return active_firmware != nullptr && active_firmware->active();
}

bool controller_configuration_transfer_active() {
    return active_configuration != nullptr && active_configuration->active();
}

bool controller_factory_transfer_active() {
    return active_factory != nullptr && active_factory->active();
}

}  // namespace firmware::target
