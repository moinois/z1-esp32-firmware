/** @file @brief Defines the per-connection buffers required before TCP admission. */
#pragma once

#include "core/protocol/protocol_constants.hpp"

#include <cstddef>

namespace firmware::application {

/// Full input and output reservation required by TCP-012 for each accepted peer.
inline constexpr std::size_t tcp_connection_buffer_capacity =
    core::protocol::host_maximum_frame_size;

/// Rejects a connection unless both complete transport reservations exist.
bool tcp_connection_capacity_available(const void* input, const void* output);

}  // namespace firmware::application
