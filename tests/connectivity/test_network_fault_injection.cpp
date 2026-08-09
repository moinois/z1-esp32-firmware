// Verifies one-shot network failures remain bound to their selected adapter.
#include "test.hpp"
#include "application/connectivity/network_fault_injection.hpp"

using firmware::application::NetworkFault;
using firmware::application::NetworkFaultInjection;

TEST_CASE(network_fault_is_consumed_only_by_its_selected_boundary) {
    NetworkFaultInjection faults;
    REQUIRE_EQ(faults.selected(), NetworkFault::none);

    faults.select(NetworkFault::discovery_open);
    REQUIRE(!faults.consume(NetworkFault::discovery_send));
    REQUIRE_EQ(faults.selected(), NetworkFault::discovery_open);
    REQUIRE(faults.consume(NetworkFault::discovery_open));
    REQUIRE_EQ(faults.selected(), NetworkFault::none);
    REQUIRE(!faults.consume(NetworkFault::discovery_open));
}

TEST_CASE(network_fault_selection_can_be_replaced_or_cleared) {
    NetworkFaultInjection faults;
    faults.select(NetworkFault::tcp_temporary_send);
    faults.select(NetworkFault::tcp_permanent_send);
    REQUIRE(!faults.consume(NetworkFault::tcp_temporary_send));
    REQUIRE(faults.consume(NetworkFault::tcp_permanent_send));

    faults.select(NetworkFault::discovery_send);
    faults.select(NetworkFault::none);
    REQUIRE_EQ(faults.selected(), NetworkFault::none);
}
