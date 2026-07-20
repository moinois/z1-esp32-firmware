// Declares deterministic connectivity identity and AP channel selection policy.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace firmware::core {

// Selects the first configured machine name or derives the station-MAC fallback.
std::string derive_machine_name(
    const std::vector<std::string>& configuration_lines,
    const std::array<std::uint8_t, 6U>& station_mac);

// Counts valid observations with wrapping bytes and selects the lowest minimum.
std::uint8_t select_access_point_channel(
    const std::vector<std::uint8_t>& observed_channels,
    bool result_storage_available = true);

}  // namespace firmware::core
