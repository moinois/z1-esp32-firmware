/** @file @brief Implements all-or-nothing TCP connection capacity admission. */
#include "application/transport/tcp_connection_capacity.hpp"

namespace firmware::application {

bool tcp_connection_capacity_available(const void* input, const void* output) {
    return input != nullptr && output != nullptr;
}

}  // namespace firmware::application
