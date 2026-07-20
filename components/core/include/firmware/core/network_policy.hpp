// Declares deterministic connectivity identity and AP channel selection policy.
#pragma once

#include "firmware/core/bytes.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
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

// Describes one raw Wi-Fi observation returned by the target scanner.
struct WifiObservation {
    ByteVector raw_ssid;
    std::int32_t rssi;
    std::uint8_t authentication_mode;
};

// Holds one retained, bounded, and deduplicated scan result.
struct WifiScanResult {
    std::string ssid;
    std::int32_t rssi;
    std::uint8_t authentication_mode;
    bool selected;
};

// Filters, deduplicates, bounds, and stably orders up to twenty observations.
std::vector<WifiScanResult> process_wifi_scan(
    const std::vector<WifiObservation>& observations,
    std::string_view selected_ssid);

// Formats retained scan results into newline-terminated host records.
std::string format_wifi_scan(const std::vector<WifiScanResult>& results);

// Identifies the operation selected by one host `wlan` command.
enum class WlanAction {
    scan,
    connect,
    disconnect,
};

// Holds the first two non-option tokens and selected WLAN action.
struct WlanCommand {
    WlanAction action = WlanAction::scan;
    std::string ssid;
    std::string password;
};

// Parses the bounded full wlan payload using pre-decode literal-space tokens.
WlanCommand parse_wlan_command(BytesView payload);

}  // namespace firmware::core
