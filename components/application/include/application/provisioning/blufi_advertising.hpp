/** @file @brief Declares the byte-exact legacy BLUFI advertising policy from BLE-002/BWF-002. */
#pragma once

#include "core/protocol/bytes.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace firmware::application {

/// Builds `MK_` followed by at most the first 23 machine-name bytes.
std::string blufi_device_name(std::string_view machine_name);

/// Packs one complete legacy advertising-data payload without a scan response.
core::ByteVector blufi_advertising_data(std::string_view device_name,
                                        std::int8_t transmit_power_dbm);

}  // namespace firmware::application
