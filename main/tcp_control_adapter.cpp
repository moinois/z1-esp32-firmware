// Implements TCP control listener configuration and bounded connection slots.
#include "tcp_control_adapter.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "firmware/core/frame.hpp"
#include "firmware/application/tcp_frame_sender.hpp"
#include "firmware/application/tcp_client_session.hpp"
#include "firmware/application/router.hpp"
#include "firmware/application/tcp_frame_dispatcher.hpp"
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
#include "firmware_update_adapter.hpp"
#include "firmware/application/filesystem_commands.hpp"
#include "firmware/application/file_upload.hpp"
#include "firmware/application/file_download.hpp"
#include "firmware/application/directory_listing.hpp"
#include "firmware/application/file_hash_command.hpp"
#include "firmware/application/configuration_files.hpp"
#include "firmware/application/configuration_get.hpp"
#include "firmware/application/configuration_set.hpp"
#include "firmware/application/m942_exercise.hpp"
#include "firmware/application/controller_snapshots.hpp"
#include "firmware/application/wlan_command.hpp"
#include "firmware/application/wlan_request.hpp"
#include "firmware/application/runtime_commands.hpp"
#include "firmware/application/serial_number.hpp"
#include "firmware/application/recording_commands.hpp"
#include "firmware/core/text.hpp"
#include "firmware/core/protocol_constants.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <cstdint>
#include <esp_timer.h>
#include <memory>
#include <string_view>
#include <utility>

namespace firmware::target {
namespace {

constexpr int control_port = 2222;
constexpr int listen_backlog = 4;
constexpr int maximum_clients = 4;
std::atomic_int active_clients{0};
std::atomic_uint32_t next_generation{1U};
firmware::application::Router tcp_router;
RecordingRequestState tcp_recording_state;
firmware::application::StationRuntime tcp_station_runtime;
firmware::application::LiveConfiguration tcp_live_configuration;

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

void forward_tcp_controller_frame(firmware::application::TcpClientSession&,
                                  const firmware::core::Frame& frame) {
    static_cast<void>(enqueue_controller_frame(frame));
}

void handle_tcp_local_frame(firmware::application::TcpClientSession& session,
                            const firmware::core::Frame& frame) {
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
        TcpPlayPreparationAdapter play_port(session);
        auto& play_session = shared_play_session();
        const bool prepared = play_session.prepare(
            frame.payload,
            static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL), play_port);
        if (prepared) {
            static_cast<void>(session.queue_frame(play_session.status_reply()));
        }
        return;
    }
    const auto match = firmware::core::recognize_command(frame.payload);
    if (match.kind == firmware::core::CommandKind::can_exercise) {
        TcpM942Port m942_port(session);
        firmware::application::M942ExerciseService exercise(m942_port);
        if (exercise.submit(session.identity(), frame, true)) {
            exercise.run();
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
            && match.kind != firmware::core::CommandKind::version) {
            return;
        }
        const std::string_view command(
            reinterpret_cast<const char*>(frame.payload.data()),
            frame.payload.size());
        if (match.kind == firmware::core::CommandKind::serial_get
            || match.kind == firmware::core::CommandKind::serial_set) {
            TcpSerialNumberAdapter serial_port(session);
            firmware::application::SerialNumberService serial_service(serial_port);
            if (match.kind == firmware::core::CommandKind::serial_get) {
                serial_service.handle_get(command);
            } else {
                serial_service.handle_set(command);
            }
        } else if (match.kind == firmware::core::CommandKind::system_time
                   || match.kind == firmware::core::CommandKind::clear_first_time) {
            TcpRuntimeCommandAdapter runtime_port(session);
            firmware::application::RuntimeCommandService runtime_service(runtime_port);
            if (match.kind == firmware::core::CommandKind::system_time) {
                runtime_service.handle_system_time(command);
            } else {
                runtime_service.handle_clear_first_boot(command);
            }
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
            if (match.kind == firmware::core::CommandKind::config_get) {
                firmware::application::ConfigurationGet::execute(
                    frame.payload, tcp_live_configuration, configuration_port);
            } else {
                firmware::application::ConfigurationSet::execute(
                    frame.payload, tcp_live_configuration, configuration_port);
            }
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
            if (match.kind == firmware::core::CommandKind::make_directory) {
                firmware::application::FilesystemCommands::make_directory(
                    frame.payload, filesystem_port);
            } else if (match.kind == firmware::core::CommandKind::remove) {
                firmware::application::FilesystemCommands::remove(
                    frame.payload, filesystem_port);
            } else if (match.kind == firmware::core::CommandKind::move) {
                firmware::application::FilesystemCommands::move(
                    frame.payload, filesystem_port);
            } else if (match.kind == firmware::core::CommandKind::list) {
                TcpDirectoryListAdapter directory_port(session);
                firmware::application::DirectoryListing::execute(
                    frame.payload, directory_port);
            } else if (match.kind == firmware::core::CommandKind::md5_sum) {
                TcpFileHashAdapter hash_port(session);
                firmware::application::FileHashCommand::execute(
                    frame.payload, hash_port);
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
    TcpFileTransferRuntime(firmware::application::TcpClientSession& session,
                            firmware::application::Router& router)
        : session_(session), router_(router), upload_port_(session),
          download_port_(session) {}

    // Starts or rejects a new upload/download command for this connection.
    void handle(const firmware::core::Frame& frame, std::uint64_t now) {
        if (frame.type == firmware::core::protocol::file_command) {
            const auto start = firmware::core::parse_file_transfer_start(frame.payload);
            if (!start.has_value()) return;
            if (upload_.active() || download_.active() ||
                !router_.ownership().claim_file(session_.identity())) {
                session_.queue_frame({firmware::core::protocol::file_cancel,
                                      firmware::core::ByteVector(
                                          firmware::application::file_owner_limit_message,
                                          firmware::application::file_owner_limit_message +
                                              std::char_traits<char>::length(
                                                  firmware::application::file_owner_limit_message))});
                return;
            }
            bool started = false;
            if (start->direction == firmware::core::FileTransferDirection::upload) {
                started = upload_.start(session_.identity(), start->path, now, upload_port_);
            } else {
                started = download_.start(session_.identity(), start->path, now, download_port_);
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

    // Releases ownership when the TCP connection is closed.
    void disconnect() {
        if (upload_.active()) upload_port_.close_files();
        if (download_.active()) download_port_.close_file();
        if (upload_.active() || download_.active() ||
            router_.ownership().is_file_owner(session_.identity())) {
            router_.ownership().release_file();
        }
    }

private:
    void release_if_finished() {
        if (!upload_.active() && !download_.active() &&
            router_.ownership().is_file_owner(session_.identity())) {
            router_.ownership().release_file();
        }
    }

    firmware::application::TcpClientSession& session_;
    firmware::application::Router& router_;
    TcpFileUploadAdapter upload_port_;
    TcpFileDownloadAdapter download_port_;
    firmware::application::FileUpload upload_;
    firmware::application::FileDownload download_;
};

struct TcpClientContext {
    int socket;
    firmware::application::HostIdentity identity;
};

bool send_tcp_bytes(int client, firmware::core::BytesView bytes) {
    firmware::application::TcpFrameSender sender;
    return sender.send(bytes, [client](firmware::core::BytesView remaining) {
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
    while (const auto* frame = session.transmit_queue().front()) {
        if (!send_tcp_bytes(client, *frame)) {
            return false;
        }
        session.transmit_queue().pop_front();
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
    const int idle_seconds = 5;
    const int interval_seconds = 3;
    const int probe_count = 1;
    setsockopt(socket, IPPROTO_TCP, TCP_KEEPIDLE, &idle_seconds, sizeof(idle_seconds));
    setsockopt(socket, IPPROTO_TCP, TCP_KEEPINTVL, &interval_seconds, sizeof(interval_seconds));
    setsockopt(socket, IPPROTO_TCP, TCP_KEEPCNT, &probe_count, sizeof(probe_count));
    timeval receive_timeout{10, 0};
    timeval send_timeout{5, 0};
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &receive_timeout, sizeof(receive_timeout));
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout));
}

void tcp_client_task(void* parameter) {
    auto* context = static_cast<TcpClientContext*>(parameter);
    const int client = context->socket;
    firmware::application::TcpClientSession session(context->identity);
    TcpFileTransferRuntime transfer_runtime(session, tcp_router);
    firmware::application::TcpFrameDispatcher dispatcher(
        tcp_router,
        firmware::application::TcpDispatchSinks{
            forward_tcp_controller_frame,
            handle_tcp_local_frame,
            [&transfer_runtime](firmware::application::TcpClientSession&,
                                const firmware::core::Frame& frame) {
                transfer_runtime.handle(frame,
                                         static_cast<std::uint64_t>(esp_timer_get_time() / 1000));
            },
            {}});
    configure_socket(client);
    std::uint8_t input[2048];
    for (;;) {
        const int count = recv(client, input, sizeof(input), 0);
        if (count <= 0) break;
        session.receive({input, static_cast<std::size_t>(count)},
            [&session, &dispatcher](const firmware::application::HostIdentity&,
                                     const firmware::core::Frame& frame) {
                dispatcher.dispatch(session, frame);
            });
        transfer_runtime.poll(
            static_cast<std::uint64_t>(esp_timer_get_time() / 1000));
        if (!drain_tcp_transmit_queue(client, session)) {
            break;
        }
    }
    transfer_runtime.disconnect();
    close(client);
    active_clients.fetch_sub(1, std::memory_order_release);
    delete context;
    vTaskDelete(nullptr);
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
        int expected = active_clients.load(std::memory_order_acquire);
        while (expected < maximum_clients &&
               !active_clients.compare_exchange_weak(
                   expected, expected + 1, std::memory_order_acq_rel)) {}
        if (expected >= maximum_clients) {
            send_rejection(client);
            close(client);
            continue;
        }
        auto* context = new TcpClientContext{
            client,
            {firmware::application::HostTransport::tcp,
             static_cast<std::uint8_t>(expected),
             next_generation.fetch_add(1U, std::memory_order_relaxed)}};
        TaskHandle_t task = nullptr;
        if (xTaskCreate(tcp_client_task, "tcp_client", 4096U, context,
                        4U, &task) != pdPASS) {
            delete context;
            close(client);
            active_clients.fetch_sub(1, std::memory_order_release);
        }
    }
}

}  // namespace

void TcpControlAdapter::start() {
    xTaskCreate(tcp_accept_task, "tcp_control", 4096U, nullptr, 4U, nullptr);
}

std::size_t active_tcp_client_count() {
    return static_cast<std::size_t>(active_clients.load(std::memory_order_acquire));
}

}  // namespace firmware::target
