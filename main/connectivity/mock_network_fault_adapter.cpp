/** @file @brief Implements deterministic socket failure controls without replacing lwIP. */
#include "mock_network_fault_adapter.hpp"

#include "hardware_adapter_factory.hpp"
#include "tcp_discovery_adapter.hpp"

namespace firmware::target {
namespace {

firmware::application::NetworkFaultInjection faults;

std::string_view fault_name(firmware::application::NetworkFault fault) {
    using firmware::application::NetworkFault;
    switch (fault) {
        case NetworkFault::discovery_open: return "discovery-open";
        case NetworkFault::discovery_send: return "discovery-send";
        case NetworkFault::tcp_temporary_send: return "tcp-temporary-send";
        case NetworkFault::tcp_permanent_send: return "tcp-permanent-send";
        case NetworkFault::none: return "none";
    }
    return "none";
}

}  // namespace

bool consume_network_fault(firmware::application::NetworkFault expected) {
    return HardwareAdapterFactory::network_faults_enabled() &&
           faults.consume(expected);
}

std::string handle_mock_network_control(std::string_view command) {
    using firmware::application::NetworkFault;
    if (!HardwareAdapterFactory::network_faults_enabled()) {
        return "mock-net unavailable in live build\n";
    }
    constexpr std::string_view prefix = "mock-net";
    std::string_view action = command.substr(
        command.size() < prefix.size() ? command.size() : prefix.size());
    while (!action.empty() && action.front() == ' ') action.remove_prefix(1U);

    NetworkFault selected = NetworkFault::none;
    if (action == "fail-discovery-open") {
        selected = NetworkFault::discovery_open;
    } else if (action == "fail-discovery-send") {
        selected = NetworkFault::discovery_send;
    } else if (action == "fail-tcp-temporary") {
        selected = NetworkFault::tcp_temporary_send;
    } else if (action == "fail-tcp-permanent") {
        selected = NetworkFault::tcp_permanent_send;
    } else if (action == "clear") {
        faults.select(NetworkFault::none);
        return "mock-net faults cleared\n";
    } else if (action == "status") {
        return std::string("mock-net pending: ") +
               std::string(fault_name(faults.selected())) + "\n";
    } else {
        return "mock-net expected fail-discovery-open, fail-discovery-send, "
               "fail-tcp-temporary, fail-tcp-permanent, clear, or status\n";
    }
    faults.select(selected);
    if (selected == NetworkFault::discovery_open) {
        recreate_tcp_discovery_socket();
    }
    return std::string("mock-net selected: ") +
           std::string(fault_name(selected)) + "\n";
}

}  // namespace firmware::target
