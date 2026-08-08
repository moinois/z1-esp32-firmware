/** @file @brief Mapping from physical CAN health to CANopen object errors. */
#pragma once

#include <cstdint>

namespace firmware::core {

/** Maps TWAI bus state and counters to CANopen communication-error bit 4. */
std::uint8_t can_error_register_from_status(bool bus_off,
                                            std::uint8_t tx_error_count,
                                            std::uint8_t rx_error_count);

}  // namespace firmware::core
