// Implements UDP broadcast sockets behind the portable discovery service.
#include "tcp_discovery_adapter.hpp"

#include "firmware/application/discovery_service.hpp"
#include "firmware/application/connectivity_defaults.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tcp_control_adapter.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string>

namespace firmware::target {
namespace {

class EspDiscoveryPort final : public firmware::application::DiscoveryPort {
public:
    bool open_long_lived_socket() override {
        if (long_socket_ >= 0) return true;
        long_socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        return long_socket_ >= 0;
    }

    bool enable_long_lived_broadcast() override {
        const int enabled = 1;
        return long_socket_ >= 0 && setsockopt(
            long_socket_, SOL_SOCKET, SO_BROADCAST, &enabled, sizeof(enabled)) == 0;
    }

    void close_long_lived_socket() override {
        if (long_socket_ >= 0) close(long_socket_);
        long_socket_ = -1;
    }

    void send_long_lived(std::string_view destination, std::uint16_t port,
                         std::string_view payload) override {
        send_datagram(long_socket_, destination, port, payload);
    }

    bool open_temporary_socket() override {
        temporary_socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (temporary_socket_ < 0) return false;
        const int enabled = 1;
        if (setsockopt(temporary_socket_, SOL_SOCKET, SO_BROADCAST,
                       &enabled, sizeof(enabled)) != 0) {
            close_temporary_socket();
            return false;
        }
        return true;
    }

    void send_temporary(std::string_view destination, std::uint16_t port,
                        std::string_view payload) override {
        send_datagram(temporary_socket_, destination, port, payload);
    }

    void close_temporary_socket() override {
        if (temporary_socket_ >= 0) close(temporary_socket_);
        temporary_socket_ = -1;
    }

    void delay_milliseconds(std::uint32_t duration) override {
        vTaskDelay(pdMS_TO_TICKS(duration));
    }

private:
    static void send_datagram(int socket_fd, std::string_view destination,
                              std::uint16_t port, std::string_view payload) {
        if (socket_fd < 0) return;
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        if (inet_aton(std::string(destination).c_str(), &address.sin_addr) == 0) {
            return;
        }
        static_cast<void>(sendto(socket_fd, payload.data(), payload.size(), 0,
                                  reinterpret_cast<sockaddr*>(&address),
                                  sizeof(address)));
    }

    int long_socket_ = -1;
    int temporary_socket_ = -1;
};

EspDiscoveryPort discovery_port;
firmware::application::DiscoveryService discovery_service(discovery_port,
                                                          std::string(
                                                              firmware::application::connectivity_defaults::network_hostname));

// Keeps periodic discovery traffic alive without coupling policy to TCP code.
void discovery_task(void*) {
    for (;;) {
        discovery_service.periodic_cycle(std::nullopt,
                                         active_tcp_client_count());
    }
}

}  // namespace

void send_tcp_discovery_burst(std::size_t active_tcp_clients) {
    discovery_service.send_command_burst({}, active_tcp_clients);
}

void update_tcp_discovery_station(std::string_view ipv4,
                                  std::string_view netmask,
                                  std::size_t active_tcp_clients) {
    discovery_service.station_address_assigned(ipv4, netmask, {},
                                                active_tcp_clients);
}

void clear_tcp_discovery_station() {
    discovery_service.station_disconnected();
}

void start_tcp_discovery_task() {
    static constexpr std::uint32_t stack_size = 3072U;
    static constexpr UBaseType_t priority = 2U;
    xTaskCreate(discovery_task, "discovery", stack_size, nullptr, priority,
                nullptr);
}

void configure_tcp_discovery_machine_name(std::string_view machine_name) {
    discovery_service.set_machine_name(std::string(machine_name));
}

}  // namespace firmware::target
