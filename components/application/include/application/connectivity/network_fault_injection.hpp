/** @file @brief Declares one-shot network failures shared by target socket adapters. */
#pragma once

#include <atomic>
#include <cstdint>

namespace firmware::application {

/** One-shot live-network boundary fault selected by the mock control command. */
enum class NetworkFault : std::uint8_t {
    none,
    discovery_open,
    discovery_send,
    tcp_temporary_send,
    tcp_permanent_send,
};

/// Stores one fault until the matching production boundary consumes it.
class NetworkFaultInjection {
public:
    /// Replaces the pending fault; selecting none clears the test boundary.
    void select(NetworkFault fault);

    /// Consumes and clears the fault only when it matches this boundary.
    bool consume(NetworkFault expected);

    /// Returns the currently pending fault for deterministic diagnostics.
    NetworkFault selected() const;

private:
    std::atomic<NetworkFault> fault_{NetworkFault::none};
};

}  // namespace firmware::application
