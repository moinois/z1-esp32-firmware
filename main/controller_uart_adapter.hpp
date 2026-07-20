// Declares the ESP-IDF adapter for complete-frame controller UART I/O.
#pragma once

#include "firmware/core/bytes.hpp"

#include <cstddef>
#include <cstdint>

namespace firmware::target {

// Provides the narrow blocking byte interface required by the controller service.
class ControllerUartAdapter {
public:
    // Configures UART1 and allocates its fixed receive and transmit buffers.
    bool initialize();

    // Reads at most the configured receive-chunk size with the configured wait.
    int read(std::uint8_t* destination, std::size_t capacity) const;

    // Submits one complete encoded frame through a single ESP-IDF write call.
    int write(core::BytesView frame) const;
};

}  // namespace firmware::target
