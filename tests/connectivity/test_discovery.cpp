// Verifies discovery formatting, socket lifecycle, ordering, and bursts.
#include "test.hpp"

#include "firmware/application/discovery_service.hpp"
#include "firmware/core/discovery_policy.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using firmware::application::DiscoveryPort;
using firmware::application::DiscoveryService;

namespace {

// Captures one datagram independently of the socket that sent it.
struct SentDatagram {
    std::string destination;
    std::uint16_t port;
    std::string payload;
};

// Records discovery socket operations and programmable creation outcomes.
class FakeDiscoveryPort final : public DiscoveryPort {
public:
    // Attempts to create the long-lived discovery socket.
    bool open_long_lived_socket() override {
        operations.emplace_back("open-long");
        return long_socket_opens;
    }

    // Records broadcast enabling and returns its ignored result.
    bool enable_long_lived_broadcast() override {
        operations.emplace_back("broadcast-long");
        return broadcast_enable_succeeds;
    }

    // Closes the long-lived socket before recreation.
    void close_long_lived_socket() override {
        operations.emplace_back("close-long");
    }

    // Records one periodic datagram; its send result is deliberately absent.
    void send_long_lived(std::string_view destination, std::uint16_t port,
                         std::string_view payload) override {
        operations.emplace_back("send-long");
        long_datagrams.push_back(
            {std::string(destination), port, std::string(payload)});
    }

    // Attempts to create one temporary burst socket.
    bool open_temporary_socket() override {
        operations.emplace_back("open-temporary");
        return temporary_socket_opens;
    }

    // Records one temporary burst datagram.
    void send_temporary(std::string_view destination, std::uint16_t port,
                        std::string_view payload) override {
        operations.emplace_back("send-temporary");
        temporary_datagrams.push_back(
            {std::string(destination), port, std::string(payload)});
    }

    // Closes a successfully created temporary socket.
    void close_temporary_socket() override {
        operations.emplace_back("close-temporary");
    }

    // Records task delays without blocking host tests.
    void delay_milliseconds(std::uint32_t duration) override {
        delays.push_back(duration);
    }

    bool long_socket_opens = true;
    bool broadcast_enable_succeeds = true;
    bool temporary_socket_opens = true;
    std::vector<std::string> operations;
    std::vector<SentDatagram> long_datagrams;
    std::vector<SentDatagram> temporary_datagrams;
    std::vector<std::uint32_t> delays;
};

}  // namespace

TEST_CASE(disc_002_machine_state_handles_absent_malformed_and_bounded_status) {
    REQUIRE_EQ(firmware::core::discovery_machine_state(std::nullopt),
               std::string("Idle"));
    REQUIRE_EQ(firmware::core::discovery_machine_state("not-a-status"),
               std::string("Unknown"));
    REQUIRE_EQ(firmware::core::discovery_machine_state("prefix<Running|X:1>"),
               std::string("Running"));
    REQUIRE_EQ(firmware::core::discovery_machine_state(
                   "<abcdefghijklmnopqrstuvwxyz123456789|X:1>"),
               std::string("abcdefghijklmnopqrstuvwxyz12345"));
}

TEST_CASE(disc_001_and_003_payload_is_exact_bounded_and_full_only_at_four) {
    REQUIRE_EQ(firmware::core::format_discovery_payload(
                   "Makera_Z1_ABCD", "192.168.4.1", 4U, "Idle"),
               std::string("Makera_Z1_ABCD,192.168.4.1,2222,1,Idle"));
    REQUIRE_EQ(firmware::core::format_discovery_payload(
                   "name", "192.168.4.1", 5U, "Idle"),
               std::string("name,192.168.4.1,2222,0,Idle"));
    REQUIRE_EQ(firmware::core::format_discovery_payload(
                   std::string(120U, 'n'), "192.168.4.1", 0U, "Idle")
                   .size(),
               127U);
}

TEST_CASE(disc_004_station_broadcast_uses_ipv4_and_netmask) {
    REQUIRE_EQ(firmware::core::ipv4_broadcast_address("10.2.3.4",
                                                       "255.255.252.0"),
               std::optional<std::string>("10.2.3.255"));
    REQUIRE(!firmware::core::ipv4_broadcast_address("bad", "255.255.255.0")
                 .has_value());
}

TEST_CASE(disc_004_periodic_cycle_sends_station_before_access_point) {
    FakeDiscoveryPort port;
    DiscoveryService service(port, "machine");
    service.station_address_assigned("10.0.0.7", "255.255.255.0",
                                     "<Ready|X:1>", 4U);
    port.operations.clear();
    port.temporary_datagrams.clear();
    port.delays.clear();

    service.periodic_cycle("<Ready|X:1>", 4U);

    REQUIRE_EQ(port.long_datagrams.size(), 2U);
    REQUIRE_EQ(port.long_datagrams[0].destination, std::string("10.0.0.255"));
    REQUIRE_EQ(port.long_datagrams[0].port, 3333U);
    REQUIRE_EQ(port.long_datagrams[0].payload,
               std::string("machine,10.0.0.7,2222,1,Ready"));
    REQUIRE_EQ(port.long_datagrams[1].destination,
               std::string("192.168.4.255"));
    REQUIRE_EQ(port.long_datagrams[1].payload,
               std::string("machine,192.168.4.1,2222,1,Ready"));
    REQUIRE_EQ(port.delays, std::vector<std::uint32_t>({500U}));
}

TEST_CASE(disc_machine_name_can_be_replaced_before_periodic_advertisement) {
    FakeDiscoveryPort port;
    DiscoveryService service(port, "initial");
    service.set_machine_name("configured-name");

    service.periodic_cycle(std::nullopt, 0U);

    REQUIRE_EQ(port.long_datagrams.size(), 1U);
    REQUIRE_EQ(port.long_datagrams.front().payload,
               std::string("configured-name,192.168.4.1,2222,0,Idle"));
}

TEST_CASE(disc_005_socket_failure_retries_and_broadcast_failure_stays_in_service) {
    FakeDiscoveryPort port;
    port.long_socket_opens = false;
    DiscoveryService service(port, "machine");

    service.periodic_cycle(std::nullopt, 0U);

    REQUIRE(port.long_datagrams.empty());
    REQUIRE_EQ(port.delays, std::vector<std::uint32_t>({2000U}));

    port.long_socket_opens = true;
    port.broadcast_enable_succeeds = false;
    service.periodic_cycle(std::nullopt, 0U);
    service.periodic_cycle(std::nullopt, 0U);

    REQUIRE_EQ(port.long_datagrams.size(), 2U);
    REQUIRE_EQ(port.delays,
               std::vector<std::uint32_t>({2000U, 500U, 500U}));
}

TEST_CASE(disc_005_and_006_address_and_disconnect_recreate_with_event_burst) {
    FakeDiscoveryPort port;
    DiscoveryService service(port, "machine");
    service.periodic_cycle(std::nullopt, 0U);
    port.operations.clear();
    port.delays.clear();

    service.station_address_assigned("172.16.1.2", "255.255.255.0",
                                     "<Idle|X:1>", 0U);

    REQUIRE_EQ(port.temporary_datagrams.size(), 3U);
    REQUIRE(port.delays.empty());
    service.periodic_cycle("<Idle|X:1>", 0U);
    REQUIRE_EQ(port.operations[4], std::string("close-temporary"));
    REQUIRE_EQ(port.operations[5], std::string("close-long"));
    REQUIRE_EQ(port.operations[6], std::string("open-long"));

    port.operations.clear();
    service.station_disconnected();
    service.periodic_cycle(std::nullopt, 0U);
    REQUIRE_EQ(port.operations[0], std::string("close-long"));
    REQUIRE_EQ(port.long_datagrams.back().destination,
               std::string("192.168.4.255"));
}

TEST_CASE(disc_socket_recreation_preserves_station_destination) {
    FakeDiscoveryPort port;
    DiscoveryService service(port, "machine");
    service.station_address_assigned("172.16.1.2", "255.255.255.0",
                                     "<Idle|X:1>", 0U);
    service.periodic_cycle("<Idle|X:1>", 0U);
    port.operations.clear();
    port.long_datagrams.clear();

    service.recreate_socket();
    service.periodic_cycle("<Idle|X:1>", 0U);

    REQUIRE_EQ(port.operations[0], std::string("close-long"));
    REQUIRE_EQ(port.operations[1], std::string("open-long"));
    REQUIRE_EQ(port.long_datagrams.size(), 2U);
    REQUIRE_EQ(port.long_datagrams.front().destination,
               std::string("172.16.1.255"));
}

TEST_CASE(disc_007_and_008_command_burst_delays_after_all_three_or_silently_omits) {
    FakeDiscoveryPort port;
    DiscoveryService service(port, "machine");
    service.station_address_assigned("192.0.2.9", "255.255.255.0",
                                     "<Run|X:1>", 1U);
    port.temporary_datagrams.clear();
    port.delays.clear();

    service.send_command_burst("<Run|X:1>", 1U);

    REQUIRE_EQ(port.temporary_datagrams.size(), 3U);
    REQUIRE_EQ(port.delays,
               std::vector<std::uint32_t>({100U, 100U, 100U}));

    port.temporary_socket_opens = false;
    port.temporary_datagrams.clear();
    port.delays.clear();
    service.send_command_burst("<Run|X:1>", 1U);
    REQUIRE(port.temporary_datagrams.empty());
    REQUIRE(port.delays.empty());
}
