// Declares periodic and burst UDP discovery behind replaceable socket APIs.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace firmware::application {

// Isolates discovery policy from UDP sockets and task delays.
class DiscoveryPort {
public:
    // Enables safe destruction through a substituted socket adapter.
    virtual ~DiscoveryPort() = default;

    // Creates the socket retained by the periodic discovery task.
    virtual bool open_long_lived_socket() = 0;

    // Enables broadcast; failure deliberately leaves the socket usable.
    virtual bool enable_long_lived_broadcast() = 0;

    // Closes the retained socket before a required recreation.
    virtual void close_long_lived_socket() = 0;

    // Sends one periodic datagram; the send result is deliberately ignored.
    virtual void send_long_lived(std::string_view destination,
                                 std::uint16_t port,
                                 std::string_view payload) = 0;

    // Creates the independent socket used by one three-copy burst.
    virtual bool open_temporary_socket() = 0;

    // Sends one burst datagram; the send result is deliberately ignored.
    virtual void send_temporary(std::string_view destination,
                                std::uint16_t port,
                                std::string_view payload) = 0;

    // Closes a successfully created temporary socket.
    virtual void close_temporary_socket() = 0;

    // Waits for periodic, retry, or inter-copy timing.
    virtual void delay_milliseconds(std::uint32_t duration) = 0;
};

// Owns discovery socket recreation and station-specific destination state.
class DiscoveryService {
public:
    // Creates discovery policy for the retained machine name.
    DiscoveryService(DiscoveryPort& port, std::string machine_name);

    // Replaces the advertised machine name before periodic traffic starts.
    void set_machine_name(std::string machine_name);

    // Sends one periodic station-first cycle or waits after creation failure.
    void periodic_cycle(std::optional<std::string_view> controller_status,
                        std::size_t active_tcp_clients);

    // Retains a station subnet, requests recreation, and sends an event burst.
    void station_address_assigned(
        std::string_view ipv4, std::string_view netmask,
        std::optional<std::string_view> controller_status,
        std::size_t active_tcp_clients);

    // Clears the station destination and requests long-lived socket recreation.
    void station_disconnected();

    // Sends the command-triggered burst with a delay after every copy.
    void send_command_burst(
        std::optional<std::string_view> controller_status,
        std::size_t active_tcp_clients);

private:
    // Sends three station datagrams through one temporary socket.
    void send_temporary_burst(
        std::optional<std::string_view> controller_status,
        std::size_t active_tcp_clients, bool delay_after_each_copy);

    DiscoveryPort& port_;
    std::string machine_name_;
    std::string station_ipv4_;
    std::string station_broadcast_;
    bool long_socket_open_ = false;
    bool recreate_long_socket_ = false;
};

}  // namespace firmware::application
