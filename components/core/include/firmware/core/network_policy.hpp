/** @file
 *  @brief Deterministic connectivity identity, scan, and WLAN command policy.
 */
#pragma once

#include "firmware/core/bytes.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace firmware::core {

/** Selects the configured machine name or derives the station-MAC fallback.
 *  @param configuration_lines Raw configuration records in file order.
 *  @param station_mac Factory station MAC used only when no name is configured.
 *  @return Stable hostname suitable for discovery and network services.
 */
std::string derive_machine_name(
    const std::vector<std::string>& configuration_lines,
    const std::array<std::uint8_t, 6U>& station_mac);

/** Selects the least-observed valid access-point channel.
 *  Counts intentionally use wrapping bytes to match the bounded target policy;
 *  ties resolve to the lowest supported channel.
 *  @param observed_channels Scanner results, including possibly invalid values.
 *  @param result_storage_available Whether allocation for counters succeeded.
 *  @return Selected channel, or the policy fallback when storage is unavailable.
 */
std::uint8_t select_access_point_channel(
    const std::vector<std::uint8_t>& observed_channels,
    bool result_storage_available = true);

/** Raw Wi-Fi observation returned by the target scanner. */
struct WifiObservation {
    /// SSID bytes before validation; embedded NUL and invalid UTF-8 are possible.
    ByteVector raw_ssid;
    /// Received signal strength in dBm.
    std::int32_t rssi;
    /// Target SDK authentication-mode identifier retained for host output.
    std::uint8_t authentication_mode;
};

/** Validated and deduplicated scan result exposed to host commands. */
struct WifiScanResult {
    /// Validated network name.
    std::string ssid;
    /// Strongest observed signal for this SSID.
    std::int32_t rssi;
    /// Authentication mode belonging to the retained strongest observation.
    std::uint8_t authentication_mode;
    /// True when this result matches the currently selected station SSID.
    bool selected;
};

/** Filters, deduplicates, bounds, and orders scanner observations.
 *  @param observations Raw observations from one physical scan.
 *  @param selected_ssid Currently selected SSID used to mark one result.
 *  @return At most the protocol-defined number of host-visible results.
 */
std::vector<WifiScanResult> process_wifi_scan(
    const std::vector<WifiObservation>& observations,
    std::string_view selected_ssid);

/** Formats retained scan results into newline-terminated host records.
 *  @param results Validated results in desired output order.
 *  @return Complete text payload for a command response.
 */
std::string format_wifi_scan(const std::vector<WifiScanResult>& results);

/** Operation selected by one host `wlan` command. */
enum class WlanAction {
    /// Enumerate observable networks without changing station state.
    scan,
    /// Associate using the supplied SSID and password.
    connect,
    /// Leave the current network and suppress supplied credentials.
    disconnect,
};

/** Parsed WLAN request after option and escape handling. */
struct WlanCommand {
    /// Requested station operation; missing tokens intentionally mean scan.
    WlanAction action = WlanAction::scan;
    /// First non-option token, used as the network identifier.
    std::string ssid;
    /// Second non-option token, used only for connection.
    std::string password;
};

/** Parses one bounded full WLAN payload.
 *  Token boundaries are found before escape decoding so encoded spaces remain
 *  part of credentials rather than becoming accidental extra arguments.
 *  @param payload Full payload beginning with the `wlan` command token.
 *  @return Selected action and the first two applicable non-option tokens.
 */
WlanCommand parse_wlan_command(BytesView payload);

}  // namespace firmware::core
