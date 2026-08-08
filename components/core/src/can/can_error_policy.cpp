/** @file @brief Implements the physical-CAN-to-CANopen error-register mapping. */
#include "firmware/core/can_error_policy.hpp"

namespace firmware::core {

std::uint8_t can_error_register_from_status(bool bus_off,
                                            std::uint8_t tx_error_count,
                                            std::uint8_t rx_error_count) {
    constexpr std::uint8_t communication_error = 0x10U;
    return bus_off || tx_error_count != 0U || rx_error_count != 0U
               ? communication_error
               : 0U;
}

}  // namespace firmware::core
