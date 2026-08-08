/** @file @brief Implements atomic one-shot network failure selection. */
#include "firmware/application/network_fault_injection.hpp"

namespace firmware::application {

void NetworkFaultInjection::select(NetworkFault fault) {
    fault_.store(fault, std::memory_order_release);
}

bool NetworkFaultInjection::consume(NetworkFault expected) {
    NetworkFault selected_fault = expected;
    return fault_.compare_exchange_strong(
        selected_fault, NetworkFault::none,
        std::memory_order_acq_rel, std::memory_order_acquire);
}

NetworkFault NetworkFaultInjection::selected() const {
    return fault_.load(std::memory_order_acquire);
}

}  // namespace firmware::application
