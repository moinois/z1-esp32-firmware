// Implements machine-name lookup/fallback and wrapping channel congestion counts.
#include "firmware/core/network_policy.hpp"

#include "firmware/core/configuration_syntax.hpp"

#include <array>
#include <cstdio>
#include <string_view>

namespace firmware::core {
namespace {

constexpr std::string_view machine_name_key = "wifi.machine_name";
constexpr std::size_t maximum_machine_name_size = 31U;
constexpr std::uint8_t first_wifi_channel = 1U;
constexpr std::uint8_t last_wifi_channel = 14U;

// Formats the exact default name from the final two station-MAC bytes.
std::string fallback_machine_name(
    const std::array<std::uint8_t, 6U>& station_mac) {
    char name[15];
    const int length = std::snprintf(name, sizeof(name), "Makera_Z1_%02X%02X",
                                     static_cast<unsigned>(station_mac[4]),
                                     static_cast<unsigned>(station_mac[5]));
    if (length < 0 || static_cast<std::size_t>(length) >= sizeof(name)) {
        return {};
    }
    return {name, static_cast<std::size_t>(length)};
}

}  // namespace

std::string derive_machine_name(
    const std::vector<std::string>& configuration_lines,
    const std::array<std::uint8_t, 6U>& station_mac) {
    for (const std::string& line : configuration_lines) {
        const auto entry = parse_sd_config_line(line);
        if (!entry.has_value() || entry->key != machine_name_key) {
            continue;
        }
        if (entry->value.empty()) {
            return fallback_machine_name(station_mac);
        }
        return entry->value.substr(0U, maximum_machine_name_size);
    }
    return fallback_machine_name(station_mac);
}

std::uint8_t select_access_point_channel(
    const std::vector<std::uint8_t>& observed_channels,
    bool result_storage_available) {
    if (!result_storage_available || observed_channels.empty()) {
        return first_wifi_channel;
    }

    std::array<std::uint8_t, last_wifi_channel> counts{};
    for (const std::uint8_t channel : observed_channels) {
        if (channel < first_wifi_channel || channel > last_wifi_channel) {
            continue;
        }
        std::uint8_t& count = counts[channel - first_wifi_channel];
        count = static_cast<std::uint8_t>(count + 1U);
    }

    std::uint8_t selected = first_wifi_channel;
    for (std::uint8_t channel = first_wifi_channel + 1U;
         channel <= last_wifi_channel; ++channel) {
        if (counts[channel - first_wifi_channel] <
            counts[selected - first_wifi_channel]) {
            selected = channel;
        }
    }
    return selected;
}

}  // namespace firmware::core
