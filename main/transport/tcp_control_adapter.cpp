/** @file @brief Implements TCP control listener configuration and bounded connection slots. */
#include "tcp_control_adapter.hpp"

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "core/protocol/frame.hpp"
#include "application/transport/tcp_frame_sender.hpp"
#include "application/transport/tcp_client_session.hpp"
#include "application/runtime/router.hpp"
#include "application/transport/tcp_frame_dispatcher.hpp"
#include "application/runtime/local_command_queue.hpp"
#include "controller_command_loop.hpp"
#include "recording_request_state.hpp"
#include "tcp_serial_number_adapter.hpp"
#include "tcp_runtime_command_adapter.hpp"
#include "tcp_filesystem_adapter.hpp"
#include "tcp_file_transfer_adapter.hpp"
#include "tcp_filesystem_query_adapter.hpp"
#include "tcp_play_adapter.hpp"
#include "tcp_configuration_file_adapter.hpp"
#include "tcp_configuration_adapter.hpp"
#include "runtime_status_adapter.hpp"
#include "canopen_target_service.hpp"
#include "play_runtime_state.hpp"
#include "tcp_wlan_scan_adapter.hpp"
#include "tcp_wlan_station_adapter.hpp"
#include "tcp_wlan_connection_adapter.hpp"
#include "tcp_discovery_adapter.hpp"
#include "mock_network_fault_adapter.hpp"
#if Z1_MOCK_CONTROL_ENABLED
#include "mock_sd_card_adapter.hpp"
#include "mock_nvs_fault_adapter.hpp"
#endif
#include "firmware_update_adapter.hpp"
#include "application/storage/filesystem_commands.hpp"
#include "application/storage/file_upload.hpp"
#include "application/storage/file_download.hpp"
#include "application/storage/directory_listing.hpp"
#include "application/storage/file_hash_command.hpp"
#include "application/configuration/configuration_files.hpp"
#include "application/configuration/configuration_get.hpp"
#include "application/configuration/configuration_set.hpp"
#include "application/runtime/m942_exercise.hpp"
#include "application/controller/controller_snapshots.hpp"
#include "application/connectivity/wlan_command.hpp"
#include "application/connectivity/wlan_request.hpp"
#include "application/runtime/runtime_commands.hpp"
#include "application/runtime/serial_number.hpp"
#include "application/web/recording_commands.hpp"
#include "core/protocol/text.hpp"
#include "core/protocol/protocol_constants.hpp"

#include <arpa/inet.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <atomic>
#include <cstdint>
#include <esp_timer.h>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <vector>
#include <utility>

namespace firmware::target {
namespace {

constexpr char log_tag[] = "TCP_CONTROL";
constexpr int control_port = 2222;
constexpr int listen_backlog = 4;
constexpr int maximum_clients = 4;
constexpr int keepalive_idle_seconds = 5;
constexpr int keepalive_interval_seconds = 3;
constexpr int keepalive_probe_count = 1;
constexpr long receive_timeout_seconds = 10L;
constexpr long send_timeout_seconds = 5L;
// Local command dispatch composes filesystem, configuration, status, and WLAN
// adapters on this task's stack. The previous 4096-byte allocation overflowed
// on a physical `version` request after the complete target was composed.
constexpr std::uint32_t client_task_stack_bytes = 8192U;
constexpr std::size_t client_receive_buffer_size = 2048U;
constexpr UBaseType_t client_task_priority = 4U;
constexpr std::uint32_t m942_task_stack_size = 6144U;
constexpr UBaseType_t m942_task_priority = 4U;
constexpr std::uint32_t nvs_task_stack_size = 6144U;
constexpr UBaseType_t nvs_task_priority = 4U;
constexpr std::uint32_t accept_task_stack_size = 4096U;
constexpr UBaseType_t accept_task_priority = 4U;
std::atomic_int active_clients{0};
std::atomic_uint32_t next_generation{1U};
firmware::application::Router tcp_router;
std::mutex tcp_slot_mutex;
std::array<bool, maximum_clients> occupied_tcp_slots{};
std::array<QueueHandle_t, maximum_clients> tcp_client_slot_queues{};
std::array<std::uint8_t, maximum_clients> tcp_client_slot_indices{};
RecordingRequestState tcp_recording_state;
firmware::application::StationRuntime tcp_station_runtime;
firmware::application::LiveConfiguration tcp_live_configuration;
std::atomic_bool m942_exercise_active{false};
std::mutex tcp_session_registry_mutex;
std::vector<firmware::application::TcpClientSession*> tcp_sessions;
QueueHandle_t tcp_nvs_command_queue = nullptr;

enum class TcpNvsCommandKind : std::uint8_t {
    serial_get,
    serial_set,
    system_time,
    clear_first_time,
};

struct TcpNvsCommandRequest {
    firmware::application::TcpClientSession* session;
    TcpNvsCommandKind kind;
    std::string_view command;
    TaskHandle_t requester;
};

// Executes flash-backed NVS work on an internal-RAM stack. TCP client tasks
// intentionally use PSRAM so four simultaneous 8 KiB protocol stacks fit, but
// ESP-IDF can make PSRAM inaccessible while the flash cache is disabled.
void tcp_nvs_command_task(void*) {
    TcpNvsCommandRequest* request = nullptr;
    for (;;) {
        if (xQueueReceive(tcp_nvs_command_queue, &request, portMAX_DELAY) != pdTRUE ||
            request == nullptr || request->session == nullptr) {
            continue;
        }
        if (request->kind == TcpNvsCommandKind::serial_get ||
            request->kind == TcpNvsCommandKind::serial_set) {
            TcpSerialNumberAdapter port(*request->session);
            firmware::application::SerialNumberService service(port);
            if (request->kind == TcpNvsCommandKind::serial_get) {
                service.handle_get(request->command);
            } else {
                service.handle_set(request->command);
            }
        } else {
            TcpRuntimeCommandAdapter port(*request->session);
            firmware::application::RuntimeCommandService service(port);
            if (request->kind == TcpNvsCommandKind::system_time) {
                service.handle_system_time(request->command);
            } else {
                service.handle_clear_first_boot(request->command);
            }
        }
        xTaskNotifyGive(request->requester);
    }
}

bool run_tcp_nvs_command(firmware::application::TcpClientSession& session,
                         TcpNvsCommandKind kind, std::string_view command) {
    if (tcp_nvs_command_queue == nullptr) return false;
    TcpNvsCommandRequest request{&session, kind, command,
                                 xTaskGetCurrentTaskHandle()};
    TcpNvsCommandRequest* queued = &request;
    if (xQueueSend(tcp_nvs_command_queue, &queued, pdMS_TO_TICKS(250U)) != pdTRUE) {
        return false;
    }
    return ulTaskNotifyTake(pdTRUE, portMAX_DELAY) != 0U;
}

// Adapts one TCP-origin M942 request to the shared CANopen SDO service.
class TcpM942Port final : public firmware::application::M942ExercisePort {
public:
    explicit TcpM942Port(firmware::application::TcpClientSession& session)
        : session_(session) {}

    void forward_to_controller(const firmware::core::Frame& frame) override {
        static_cast<void>(enqueue_controller_frame(frame));
    }

    void respond(const firmware::application::HostIdentity& host,
                 const firmware::core::Frame& frame) override {
        if (host == session_.identity()) static_cast<void>(session_.queue_frame(frame));
    }

    std::uint64_t monotonic_milliseconds() const override {
        return static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL);
    }

    void delay_milliseconds(std::uint32_t duration) override {
        vTaskDelay(pdMS_TO_TICKS(duration));
    }

    void lock_sdo_client() override {}
    void unlock_sdo_client() override {}

    std::optional<std::uint32_t> read_remote_u32(
        std::uint8_t node, std::uint16_t index, std::uint8_t subindex,
        std::uint32_t timeout, std::uint64_t) override {
        auto* service = active_canopen_target_service();
        return service == nullptr
                   ? std::nullopt
                   : service->read_remote_u32(node, index, subindex, timeout);
    }

    bool write_remote_u32(std::uint8_t node, std::uint16_t index,
                          std::uint8_t subindex, std::uint32_t value,
                          std::uint32_t timeout, std::uint64_t) override {
        auto* service = active_canopen_target_service();
        return service != nullptr && service->write_remote_u32(
            node, index, subindex, value, timeout);
    }

private:
    firmware::application::TcpClientSession& session_;
};

// Owns one asynchronous M942 execution until its worker task terminates.
struct TcpM942WorkerContext {
    firmware::application::TcpClientSession* session;
    std::unique_ptr<TcpM942Port> port;
    std::unique_ptr<firmware::application::M942ExerciseService> service;
    TaskHandle_t owner_task = nullptr;
};

void tcp_m942_worker(void* parameter) {
    auto* context = static_cast<TcpM942WorkerContext*>(parameter);
    context->service->run();
    m942_exercise_active.store(false, std::memory_order_release);
    if (context->owner_task != nullptr) {
        xTaskNotifyGive(context->owner_task);
    }
    delete context;
    vTaskDelete(nullptr);
}

void forward_tcp_controller_frame(firmware::application::TcpClientSession&,
                                  const firmware::core::Frame& frame) {
    static_cast<void>(enqueue_controller_frame(frame));
}

void handle_tcp_local_frame(firmware::application::TcpClientSession& session,
                            const firmware::core::Frame& frame,
                            TaskHandle_t* m942_worker_handle) {
    if (frame.type == firmware::core::protocol::single_command &&
        !frame.payload.empty() && frame.payload.front() == '?') {
        RuntimeStatusAdapter status_sources(tcp_router);
        firmware::application::AggregatedStatusService status_service(status_sources);
        const auto response = shared_controller_snapshots().status_reply(
            status_service.extension());
        if (response.has_value()) static_cast<void>(session.queue_frame(*response));
        return;
    }
    if (frame.type != firmware::core::protocol::general_command) {
        return;
    }
    const bool is_play_command = frame.payload.size() >= 4U &&
        frame.payload[0] == 'p' && frame.payload[1] == 'l' &&
        frame.payload[2] == 'a' && frame.payload[3] == 'y';
    if (is_play_command) {
        if (!tcp_router.ownership().claim_play(session.identity())) {
            return;
        }
        TcpPlayPreparationAdapter play_port(session);
        auto& play_session = shared_play_session();
        const bool prepared = play_session.prepare(
            frame.payload,
            static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL), play_port);
        if (prepared) {
            static_cast<void>(session.queue_frame(play_session.status_reply()));
        } else {
            tcp_router.ownership().release_play();
        }
        return;
    }
    const auto match = firmware::core::recognize_command(frame.payload);
    if (match.kind == firmware::core::CommandKind::can_exercise) {
        const bool capacity = !m942_exercise_active.exchange(
            true, std::memory_order_acq_rel);
        auto context = std::make_unique<TcpM942WorkerContext>();
        context->session = &session;
        context->port = std::make_unique<TcpM942Port>(session);
        context->service = std::make_unique<
            firmware::application::M942ExerciseService>(*context->port);
        if (!context->service->submit(session.identity(), frame, capacity)) {
            if (capacity) {
                m942_exercise_active.store(false, std::memory_order_release);
            }
            return;
        }
        TcpM942WorkerContext* raw_context = context.release();
        TaskHandle_t worker = nullptr;
        if (xTaskCreate(tcp_m942_worker, "m942", m942_task_stack_size,
                        raw_context, m942_task_priority, &worker) != pdPASS) {
            m942_exercise_active.store(false, std::memory_order_release);
            delete raw_context;
            return;
        }
        raw_context->owner_task = xTaskGetCurrentTaskHandle();
        if (m942_worker_handle != nullptr) {
            *m942_worker_handle = worker;
        }
        return;
    }
    if (match.kind == firmware::core::CommandKind::upgrade ||
        match.kind == firmware::core::CommandKind::reset) {
        request_firmware_update_processing();
        return;
    }
    if (match.kind != firmware::core::CommandKind::record_start
        && match.kind != firmware::core::CommandKind::record_stop) {
        if (match.kind != firmware::core::CommandKind::serial_get
            && match.kind != firmware::core::CommandKind::serial_set
            && match.kind != firmware::core::CommandKind::system_time
            && match.kind != firmware::core::CommandKind::clear_first_time
            && match.kind != firmware::core::CommandKind::wlan
            && match.kind != firmware::core::CommandKind::make_directory
            && match.kind != firmware::core::CommandKind::remove
            && match.kind != firmware::core::CommandKind::move
            && match.kind != firmware::core::CommandKind::file_type
            && match.kind != firmware::core::CommandKind::list
            && match.kind != firmware::core::CommandKind::md5_sum
            && match.kind != firmware::core::CommandKind::config_restore
            && match.kind != firmware::core::CommandKind::config_default
            && match.kind != firmware::core::CommandKind::config_get
            && match.kind != firmware::core::CommandKind::config_set
            && match.kind != firmware::core::CommandKind::diagnose
            && match.kind != firmware::core::CommandKind::mock_sd_control
            && match.kind != firmware::core::CommandKind::mock_nvs_control
            && match.kind != firmware::core::CommandKind::mock_network_control
            && match.kind != firmware::core::CommandKind::version) {
            return;
        }
        const std::string_view command(
            reinterpret_cast<const char*>(frame.payload.data()),
            frame.payload.size());
        if (match.kind == firmware::core::CommandKind::serial_get
            || match.kind == firmware::core::CommandKind::serial_set) {
            static_cast<void>(run_tcp_nvs_command(
                session,
                match.kind == firmware::core::CommandKind::serial_get
                    ? TcpNvsCommandKind::serial_get
                    : TcpNvsCommandKind::serial_set,
                command));
        } else if (match.kind == firmware::core::CommandKind::system_time
                   || match.kind == firmware::core::CommandKind::clear_first_time) {
            static_cast<void>(run_tcp_nvs_command(
                session,
                match.kind == firmware::core::CommandKind::system_time
                    ? TcpNvsCommandKind::system_time
                    : TcpNvsCommandKind::clear_first_time,
                command));
        } else if (match.kind == firmware::core::CommandKind::wlan) {
            const auto request = firmware::application::parse_wlan_request(command);
            if (request.kind == firmware::application::WlanRequestKind::scan) {
                TcpWlanScanAdapter wlan_port(session);
                firmware::application::WlanScanCommand::execute(wlan_port);
            } else if (request.kind == firmware::application::WlanRequestKind::disconnect) {
                TcpWlanStationAdapter station;
                TcpWlanConnectionAdapter responses(session);
                firmware::application::WlanConnectionCommand::disconnect(
                    tcp_station_runtime, station, responses);
                if (tcp_station_runtime.state ==
                    firmware::application::StationConnectionState::idle) {
                    clear_tcp_discovery_station();
                }
            } else {
                TcpWlanStationAdapter station;
                TcpWlanConnectionAdapter responses(session);
                firmware::application::WlanConnectionCommand::connect(
                    tcp_station_runtime, station, responses,
                    request.ssid, request.password);
                if (tcp_station_runtime.state ==
                    firmware::application::StationConnectionState::address_ready) {
                    update_tcp_discovery_station(
                        tcp_station_runtime.ipv4, station.current_netmask(),
                        active_tcp_client_count());
                    send_tcp_discovery_burst(active_tcp_client_count());
                }
            }
        } else if (match.kind == firmware::core::CommandKind::config_restore ||
                   match.kind == firmware::core::CommandKind::config_default) {
            TcpConfigurationFileAdapter configuration_port(session);
            if (match.kind == firmware::core::CommandKind::config_restore) {
                firmware::application::ConfigurationFiles::restore(configuration_port);
            } else {
                firmware::application::ConfigurationFiles::save_default(
                    configuration_port);
            }
        } else if (match.kind == firmware::core::CommandKind::config_get ||
                   match.kind == firmware::core::CommandKind::config_set) {
            TcpConfigurationAdapter configuration_port(session);
            const firmware::core::BytesView argument(
                frame.payload.data() + match.argument_offset,
                frame.payload.size() - match.argument_offset);
            if (match.kind == firmware::core::CommandKind::config_get) {
                firmware::application::ConfigurationGet::execute(
                    argument, tcp_live_configuration, configuration_port);
            } else {
                firmware::application::ConfigurationSet::execute(
                    argument, tcp_live_configuration, configuration_port);
            }
#if Z1_MOCK_CONTROL_ENABLED
        } else if (match.kind == firmware::core::CommandKind::mock_sd_control) {
            const std::string response = handle_mock_sd_control(command);
            static_cast<void>(session.queue_frame(
                {firmware::core::protocol::text_response,
                 {response.begin(), response.end()}}));
        } else if (match.kind == firmware::core::CommandKind::mock_nvs_control) {
            const std::string response = handle_mock_nvs_control(command);
            static_cast<void>(session.queue_frame(
                {firmware::core::protocol::text_response,
                 {response.begin(), response.end()}}));
        } else if (match.kind == firmware::core::CommandKind::mock_network_control) {
            const std::string response = handle_mock_network_control(command);
            static_cast<void>(session.queue_frame(
                {firmware::core::protocol::text_response,
                 {response.begin(), response.end()}}));
#endif
        } else if (match.kind == firmware::core::CommandKind::diagnose ||
                   match.kind == firmware::core::CommandKind::version) {
            if (match.kind == firmware::core::CommandKind::diagnose) {
                RuntimeStatusAdapter status_sources(tcp_router);
                const auto response = shared_controller_snapshots().diagnostic_reply(
                    firmware::application::AggregatedStatusService(status_sources)
                        .diagnostic_rssi());
                if (response.has_value()) {
                    static_cast<void>(session.queue_frame(*response));
                }
            } else {
                static_cast<void>(session.queue_frame(
                    shared_controller_snapshots().version_reply()));
            }
        } else {
            TcpFilesystemAdapter filesystem_port(session);
            const firmware::core::BytesView argument(
                frame.payload.data() + match.argument_offset,
                frame.payload.size() - match.argument_offset);
            if (match.kind == firmware::core::CommandKind::make_directory) {
                firmware::application::FilesystemCommands::make_directory(
                    argument, filesystem_port);
            } else if (match.kind == firmware::core::CommandKind::remove) {
                firmware::application::FilesystemCommands::remove(
                    argument, filesystem_port);
            } else if (match.kind == firmware::core::CommandKind::move) {
                firmware::application::FilesystemCommands::move(
                    argument, filesystem_port);
            } else if (match.kind == firmware::core::CommandKind::list) {
                TcpDirectoryListAdapter directory_port(session);
                firmware::application::DirectoryListing::execute(
                    argument, directory_port);
            } else if (match.kind == firmware::core::CommandKind::md5_sum) {
                TcpFileHashAdapter hash_port(session);
                firmware::application::FileHashCommand::execute(
                    argument, hash_port);
            } else {
                firmware::application::FilesystemCommands::file_type(filesystem_port);
            }
        }
        return;
    }
    const auto result = firmware::application::handle_recording_command(
        match.kind, tcp_recording_state.requested());
    tcp_recording_state.set_requested(result.requested);
    static_cast<void>(session.queue_frame(result.response));
}

// Owns one host transfer state machine and its replaceable target ports.
class TcpFileTransferRuntime final {
public:
    explicit TcpFileTransferRuntime(firmware::application::Router& router)
        : router_(router) {}

    // Rebinds the logical slot to its new physical connection and repeats the
    // outstanding protocol boundary. The state machine and POSIX handles are
    // deliberately slot-owned so a brief socket loss does not destroy them.
    void bind(firmware::application::TcpClientSession& session,
              std::uint64_t now) {
        session_ = &session;
        upload_port_.bind(session_);
        download_port_.bind(session_);
        if (upload_.active()) upload_.resume(now, upload_port_);
        if (download_.active()) download_.resume(now, download_port_);
        release_if_finished();
    }

    // Starts or rejects a new upload/download command for this connection.
    void handle(const firmware::core::Frame& frame, std::uint64_t now) {
        if (session_ == nullptr) return;
        if (frame.type == firmware::core::protocol::file_command) {
            const auto start = firmware::core::parse_file_transfer_start(frame.payload);
            if (!start.has_value()) return;
            if (upload_.active() || download_.active() ||
                !router_.ownership().claim_file(session_->identity())) {
                session_->queue_frame({firmware::core::protocol::file_cancel,
                                      firmware::core::ByteVector(
                                          firmware::application::file_owner_limit_message,
                                          firmware::application::file_owner_limit_message +
                                              std::char_traits<char>::length(
                                                  firmware::application::file_owner_limit_message))});
                return;
            }
            bool started = false;
            if (start->direction == firmware::core::FileTransferDirection::upload) {
                started = upload_.start(session_->identity(), start->path, now, upload_port_);
            } else {
                started = download_.start(session_->identity(), start->path, now, download_port_);
            }
            if (!started) router_.ownership().release_file();
            return;
        }
        if (upload_.active()) upload_.handle(frame, now, upload_port_);
        if (download_.active()) download_.handle(frame, now, download_port_);
        release_if_finished();
    }

    // Advances timeout and retry policy for the active operation.
    void poll(std::uint64_t now) {
        if (upload_.active()) upload_.poll(now, upload_port_);
        if (download_.active()) download_.poll(now, download_port_);
        release_if_finished();
    }

    // Detaches only the physical connection. OWN-008 requires the logical
    // transfer, its ownership, and its open files to survive slot reuse until
    // normal completion, cancellation, protocol abort, or inactivity timeout.
    void disconnect() {
        upload_port_.bind(nullptr);
        download_port_.bind(nullptr);
        session_ = nullptr;
    }

private:
    void release_if_finished() {
        if (!upload_.active() && !download_.active() &&
            session_ != nullptr &&
            router_.ownership().is_file_owner(session_->identity())) {
            router_.ownership().release_file();
        }
    }

    firmware::application::TcpClientSession* session_ = nullptr;
    firmware::application::Router& router_;
    TcpFileUploadAdapter upload_port_;
    TcpFileDownloadAdapter download_port_;
    firmware::application::FileUpload upload_;
    firmware::application::FileDownload download_;
};

TcpFileTransferRuntime& transfer_runtime_for_slot(std::uint8_t slot) {
    static std::array<std::unique_ptr<TcpFileTransferRuntime>, maximum_clients>
        runtimes;
    auto& runtime = runtimes[slot];
    if (runtime == nullptr) {
        runtime = std::make_unique<TcpFileTransferRuntime>(tcp_router);
    }
    return *runtime;
}

struct TcpClientContext {
    int socket;
    firmware::application::HostIdentity identity;
};

bool send_tcp_bytes(int client, firmware::core::BytesView bytes) {
    firmware::application::TcpFrameSender sender;
    return sender.send(bytes, [client](firmware::core::BytesView remaining) {
        if (consume_network_fault(
                firmware::application::NetworkFault::tcp_temporary_send)) {
            return firmware::application::TcpSendResult{
                firmware::application::TcpSendStatus::temporary_failure, 0U};
        }
        if (consume_network_fault(
                firmware::application::NetworkFault::tcp_permanent_send)) {
            return firmware::application::TcpSendResult{
                firmware::application::TcpSendStatus::permanent_failure, 0U};
        }
        const ssize_t result = send(client, remaining.data(), remaining.size(), 0);
        if (result > 0) {
            return firmware::application::TcpSendResult{
                firmware::application::TcpSendStatus::sent,
                static_cast<std::size_t>(result)};
        }
        if (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK
                           || errno == EINTR)) {
            return firmware::application::TcpSendResult{
                firmware::application::TcpSendStatus::temporary_failure, 0U};
        }
        return firmware::application::TcpSendResult{
            firmware::application::TcpSendStatus::permanent_failure, 0U};
    });
}

bool drain_tcp_transmit_queue(
    int client, firmware::application::TcpClientSession& session) {
    while (session.has_pending_transmit_frame()) {
        if (!session.send_next_transmit_frame(
                [client](firmware::core::BytesView frame) {
                    return send_tcp_bytes(client, frame);
                })) {
            return false;
        }
    }
    return true;
}

void send_rejection(int client) {
    constexpr std::string_view message =
        "The maximum number of client connections has been reached. Please close other client first.";
    const firmware::core::Frame frame{
        firmware::core::protocol::ownership_limit,
        firmware::core::ByteVector(message.begin(), message.end())};
    const auto encoded = firmware::core::encode_frame(frame);
    timeval rejection_timeout{1, 0};
    setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &rejection_timeout,
               sizeof(rejection_timeout));
    firmware::application::TcpFrameSender sender;
    sender.send(encoded, [client](firmware::core::BytesView remaining) {
        const ssize_t result = send(client, remaining.data(), remaining.size(), 0);
        if (result > 0) {
            return firmware::application::TcpSendResult{
                firmware::application::TcpSendStatus::sent,
                static_cast<std::size_t>(result)};
        }
        if (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK
                           || errno == EINTR)) {
            return firmware::application::TcpSendResult{
                firmware::application::TcpSendStatus::temporary_failure, 0U};
        }
        return firmware::application::TcpSendResult{
            firmware::application::TcpSendStatus::permanent_failure, 0U};
    });
    vTaskDelay(pdMS_TO_TICKS(300U));
}

void configure_socket(int socket) {
    const int enabled = 1;
    setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled));
    setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &enabled, sizeof(enabled));
    setsockopt(socket, IPPROTO_TCP, TCP_KEEPIDLE, &keepalive_idle_seconds,
               sizeof(keepalive_idle_seconds));
    setsockopt(socket, IPPROTO_TCP, TCP_KEEPINTVL, &keepalive_interval_seconds,
               sizeof(keepalive_interval_seconds));
    setsockopt(socket, IPPROTO_TCP, TCP_KEEPCNT, &keepalive_probe_count,
               sizeof(keepalive_probe_count));
    // Keep the normative 10-second socket option. The select-driven service
    // pulse below advances application timers without weakening TCP-002.
    timeval receive_timeout{receive_timeout_seconds, 0};
    timeval send_timeout{send_timeout_seconds, 0};
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &receive_timeout, sizeof(receive_timeout));
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout));
}

void serve_tcp_client(TcpClientContext* context) {
    const int client = context->socket;
    ESP_LOGD(log_tag, "client start fd=%d generation=%lu active=%d", client,
             static_cast<unsigned long>(context->identity.generation),
             active_clients.load(std::memory_order_acquire));
    firmware::application::TcpClientSession session(context->identity);
    {
        std::lock_guard<std::mutex> lock(tcp_session_registry_mutex);
        tcp_sessions.push_back(&session);
    }
    TcpFileTransferRuntime& transfer_runtime =
        transfer_runtime_for_slot(context->identity.slot);
    transfer_runtime.bind(
        session, static_cast<std::uint64_t>(esp_timer_get_time() / 1000));
    firmware::application::LocalCommandQueue local_commands;
    TaskHandle_t m942_worker = nullptr;
    firmware::application::TcpFrameDispatcher dispatcher(
        tcp_router,
        firmware::application::TcpDispatchSinks{
            forward_tcp_controller_frame,
            [&local_commands](firmware::application::TcpClientSession&,
                           const firmware::core::Frame& local_frame) {
                static_cast<void>(local_commands.enqueue(local_frame));
            },
            [&transfer_runtime](firmware::application::TcpClientSession&,
                                const firmware::core::Frame& frame) {
                transfer_runtime.handle(frame,
                                         static_cast<std::uint64_t>(esp_timer_get_time() / 1000));
            },
            {}});
    configure_socket(client);
    std::uint8_t input[client_receive_buffer_size];
    bool transport_healthy = drain_tcp_transmit_queue(client, session);
    while (transport_healthy) {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(client, &readable);
        timeval poll_interval{0, 50'000};
        const int readiness = select(client + 1, &readable, nullptr, nullptr,
                                     &poll_interval);
        if (readiness < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (readiness == 0) {
            // File-transfer retry and inactivity timers must advance while no
            // bytes arrive. Blocking in the specified 10-second recv timeout
            // previously made the 5.010-second upload retry impossible.
            transfer_runtime.poll(
                static_cast<std::uint64_t>(esp_timer_get_time() / 1000));
            transport_healthy = drain_tcp_transmit_queue(client, session);
            continue;
        }
        const int count = recv(client, input, sizeof(input), 0);
        if (count <= 0) {
            if (count < 0 &&
                (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
                // TCP-005 treats receive timeout as temporary. Polling here is
                // what lets HFT-022 emit its 5.010-second retry while no Wi-Fi
                // packets arrive, instead of destroying the transfer state.
                transfer_runtime.poll(static_cast<std::uint64_t>(
                    esp_timer_get_time() / 1000));
                transport_healthy = drain_tcp_transmit_queue(client, session);
                continue;
            }
            if (count < 0) {
                ESP_LOGD(log_tag,
                         "client recv ended fd=%d generation=%lu errno=%d active=%d",
                         client,
                         static_cast<unsigned long>(context->identity.generation),
                         errno, active_clients.load(std::memory_order_acquire));
            } else {
                ESP_LOGD(log_tag,
                         "client peer closed fd=%d generation=%lu active=%d",
                         client,
                         static_cast<unsigned long>(context->identity.generation),
                         active_clients.load(std::memory_order_acquire));
            }
            break;
        }
        tcp_router.set_controller_transfer_active(
            controller_firmware_transfer_active() ||
            controller_configuration_transfer_active() ||
            controller_factory_transfer_active());
        session.receive({input, static_cast<std::size_t>(count)},
            [&session, &dispatcher](const firmware::application::HostIdentity&,
                                     const firmware::core::Frame& frame) {
                                     dispatcher.dispatch(session, frame);
            });
        if (const auto local_frame = local_commands.dequeue();
            local_frame.has_value()) {
            handle_tcp_local_frame(session, *local_frame, &m942_worker);
            vTaskDelay(pdMS_TO_TICKS(10U));
        }
        transfer_runtime.poll(
            static_cast<std::uint64_t>(esp_timer_get_time() / 1000));
        if (!drain_tcp_transmit_queue(client, session)) {
            break;
        }
    }
    if (m942_worker != nullptr) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
    transfer_runtime.disconnect();
    tcp_router.ownership().transport_disconnected(context->identity);
    {
        std::lock_guard<std::mutex> lock(tcp_session_registry_mutex);
        tcp_sessions.erase(std::remove(tcp_sessions.begin(), tcp_sessions.end(),
                                       &session), tcp_sessions.end());
    }
    close(client);
    {
        std::lock_guard<std::mutex> lock(tcp_slot_mutex);
        occupied_tcp_slots[context->identity.slot] = false;
    }
    const int previous = active_clients.fetch_sub(1, std::memory_order_acq_rel);
    ESP_LOGD(log_tag, "client stop fd=%d generation=%lu active=%d", client,
             static_cast<unsigned long>(context->identity.generation), previous - 1);
    delete context;
}

// Reuses one permanently allocated PSRAM stack for every connection assigned
// to a logical slot. ESP-IDF's WithCaps self-delete path creates an additional
// temporary cleanup task for every disconnect; sustained connection churn can
// exhaust those resources and leave TCP/HTTP unavailable while USB remains
// healthy. Permanent workers retain the same four-slot admission policy without
// allocating or destroying a FreeRTOS task for each socket.
void tcp_client_task(void* parameter) {
    const auto slot = *static_cast<const std::uint8_t*>(parameter);
    TcpClientContext* context = nullptr;
    for (;;) {
        if (xQueueReceive(tcp_client_slot_queues[slot], &context, portMAX_DELAY) !=
                pdTRUE ||
            context == nullptr) {
            continue;
        }
        serve_tcp_client(context);
        context = nullptr;
    }
}

void tcp_accept_task(void*) {
    const int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener < 0) {
        vTaskDelete(nullptr);
        return;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(control_port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    const int enabled = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));
    if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
        listen(listener, listen_backlog) < 0) {
        close(listener);
        vTaskDelete(nullptr);
        return;
    }
    for (;;) {
        const int client = accept(listener, nullptr, nullptr);
        if (client < 0) {
            vTaskDelay(pdMS_TO_TICKS(1000U));
            continue;
        }
        std::optional<std::uint8_t> slot;
        {
            std::lock_guard<std::mutex> lock(tcp_slot_mutex);
            for (std::uint8_t candidate = 0U; candidate < maximum_clients;
                 ++candidate) {
                if (!occupied_tcp_slots[candidate]) {
                    occupied_tcp_slots[candidate] = true;
                    slot = candidate;
                    break;
                }
            }
        }
        if (!slot.has_value()) {
            ESP_LOGW(log_tag, "client rejected fd=%d active=%d", client,
                     active_clients.load(std::memory_order_acquire));
            send_rejection(client);
            close(client);
            continue;
        }
        active_clients.fetch_add(1, std::memory_order_acq_rel);
        auto* context = new TcpClientContext{
            client,
            {firmware::application::HostTransport::tcp,
             *slot,
             next_generation.fetch_add(1U, std::memory_order_relaxed)}};
        if (tcp_client_slot_queues[*slot] == nullptr ||
            xQueueSend(tcp_client_slot_queues[*slot], &context, 0U) != pdTRUE) {
            ESP_LOGE(log_tag, "client slot dispatch failed fd=%d slot=%u active=%d",
                     client, static_cast<unsigned>(*slot),
                     active_clients.load(std::memory_order_acquire));
            delete context;
            close(client);
            active_clients.fetch_sub(1, std::memory_order_release);
            std::lock_guard<std::mutex> lock(tcp_slot_mutex);
            occupied_tcp_slots[*slot] = false;
        }
    }
}

}  // namespace

void TcpControlAdapter::start() {
    tcp_nvs_command_queue = xQueueCreate(maximum_clients, sizeof(void*));
    if (tcp_nvs_command_queue == nullptr ||
        xTaskCreate(tcp_nvs_command_task, "tcp_nvs", nvs_task_stack_size,
                    nullptr, nvs_task_priority, nullptr) != pdPASS) {
        ESP_LOGE(log_tag, "TCP NVS worker allocation failed");
    }
    bool workers_ready = true;
    for (std::uint8_t slot = 0U; slot < maximum_clients; ++slot) {
        tcp_client_slot_indices[slot] = slot;
        tcp_client_slot_queues[slot] = xQueueCreate(1U, sizeof(void*));
        TaskHandle_t task = nullptr;
        BaseType_t created = tcp_client_slot_queues[slot] == nullptr
                                 ? pdFAIL
                                 : xTaskCreateWithCaps(
                                       tcp_client_task, "tcp_client",
                                       client_task_stack_bytes,
                                       &tcp_client_slot_indices[slot],
                                       client_task_priority, &task,
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (created != pdPASS) {
            ESP_LOGE(log_tag,
                     "permanent TCP worker allocation failed slot=%u internal_heap=%u",
                     static_cast<unsigned>(slot),
                     static_cast<unsigned>(
                         heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
            workers_ready = false;
        }
    }
    if (workers_ready) {
        xTaskCreate(tcp_accept_task, "tcp_control", accept_task_stack_size,
                    nullptr, accept_task_priority, nullptr);
    }
}

std::size_t active_tcp_client_count() {
    return static_cast<std::size_t>(active_clients.load(std::memory_order_acquire));
}

void tcp_router_play_ownership_release() {
    tcp_router.ownership().release_play();
}

void tcp_router_usb_disconnected() {
    tcp_router.ownership().transport_disconnected(
        {firmware::application::HostTransport::usb, 0U, 0U});
}

void broadcast_tcp_frame(const firmware::core::Frame& frame) {
    std::lock_guard<std::mutex> lock(tcp_session_registry_mutex);
    for (auto* session : tcp_sessions) {
        if (session != nullptr) {
            static_cast<void>(session->queue_frame(frame));
        }
    }
}

firmware::application::Router& shared_host_router() {
    return tcp_router;
}

bool claim_m942_worker() {
    return !m942_exercise_active.exchange(true, std::memory_order_acq_rel);
}

void release_m942_worker() {
    m942_exercise_active.store(false, std::memory_order_release);
}

}  // namespace firmware::target
