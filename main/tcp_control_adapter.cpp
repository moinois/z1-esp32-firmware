// Implements TCP control listener configuration and bounded connection slots.
#include "tcp_control_adapter.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

namespace firmware::target {
namespace {

constexpr int control_port = 2222;
constexpr int listen_backlog = 4;
constexpr int maximum_clients = 4;

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
    int clients[maximum_clients] = {-1, -1, -1, -1};
    for (;;) {
        const int client = accept(listener, nullptr, nullptr);
        if (client < 0) {
            vTaskDelay(pdMS_TO_TICKS(1000U));
            continue;
        }
        bool stored = false;
        for (int& slot : clients) {
            if (slot < 0) {
                configure_socket(client);
                slot = client;
                stored = true;
                break;
            }
        }
        if (!stored) close(client);
    }
}

}  // namespace

void TcpControlAdapter::start() {
    xTaskCreate(tcp_accept_task, "tcp_control", 4096U, nullptr, 4U, nullptr);
}

}  // namespace firmware::target
