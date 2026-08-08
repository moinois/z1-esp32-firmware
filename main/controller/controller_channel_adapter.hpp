/** @file @brief Declares the common byte channel used by live and mock controllers. */
#pragma once

#include "firmware/core/bytes.hpp"

#include <cstddef>
#include <cstdint>

namespace firmware::target {

/// Isolates controller protocol services from the selected UART implementation.
class ControllerChannelAdapter {
public:
    /// Enables safe destruction through either hardware implementation.
    virtual ~ControllerChannelAdapter() = default;

    /// Starts the selected controller channel.
    virtual bool initialize() = 0;

    /// Reads one bounded chunk of encoded controller traffic.
    virtual int read(std::uint8_t* destination, std::size_t capacity) = 0;

    /// Submits one complete encoded controller frame.
    virtual int write(firmware::core::BytesView frame) = 0;
};

}  // namespace firmware::target
