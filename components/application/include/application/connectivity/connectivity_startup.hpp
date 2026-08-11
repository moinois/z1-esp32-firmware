/** @file @brief Declares deterministic connectivity startup orchestration behind a Wi-Fi port. */
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace firmware::application {

/// Identifies the required radio band without depending on ESP-IDF enums.
enum class WifiBand {
    ghz_2_4,
};

/// Holds the exact congestion scan settings used before concurrent mode.
struct StartupScanConfig {
    bool blocking = false;
    bool active = false;
    bool include_hidden = false;
    std::uint16_t minimum_active_dwell_milliseconds = 0U;
    std::uint16_t maximum_active_dwell_milliseconds = 0U;
};

/// Distinguishes the nonfatal allocation fallback from fatal scan API failures.
enum class StartupScanState {
    success,
    allocation_failure,
    start_failure,
    count_failure,
    records_failure,
};

/// Holds observed channels or the stage at which startup scanning failed.
struct StartupScanOutcome {
    StartupScanState state;
    std::vector<std::uint8_t> observed_channels;
};

/// Holds the complete AP/station network configuration selected by policy.
struct ConnectivityStartupConfig {
    std::string access_point_ssid;
    bool visible = false;
    bool open_authentication = false;
    std::string password;
    WifiBand band = WifiBand::ghz_2_4;
    std::uint8_t maximum_clients = 0U;
    std::uint8_t channel = 0U;
    bool power_save_enabled = true;
    std::uint8_t global_socket_capacity = 0U;
    std::string default_hostname;
    std::string access_point_ipv4;
    std::uint8_t access_point_prefix_length = 0U;
    bool access_point_dhcp_server = false;
    bool station_dhcp_client = false;
};

/// Holds normalized values loaded from the persistent `softap` namespace.
struct AccessPointStartupSettings {
    std::optional<std::uint8_t> saved_channel;
    std::string password;
    bool enabled = true;
};

/** Normalizes current string storage and the legacy unsigned channel form.
 *
 * A present string is authoritative even when invalid. The legacy value is
 * consulted only when no string value could be read, matching the fact that
 * one NVS key cannot simultaneously have both types.
 */
AccessPointStartupSettings parse_access_point_startup_settings(
    std::optional<std::string_view> channel,
    std::optional<std::uint8_t> legacy_channel,
    std::optional<std::string_view> password,
    std::optional<std::uint8_t> enabled);

/// Isolates startup sequencing from ESP-IDF Wi-Fi and network-interface APIs.
class ConnectivityStartupPort {
public:
    /// Enables safe destruction through a substituted startup adapter.
    virtual ~ConnectivityStartupPort() = default;

    /// Enters station-only mode before the channel congestion scan.
    virtual bool enter_station_only_mode() = 0;

    /// Performs the blocking startup scan and reports its precise outcome.
    virtual StartupScanOutcome scan_channels(
        const StartupScanConfig& config) = 0;

    /// Applies and starts the selected concurrent AP/station configuration.
    virtual bool start_access_point_and_station(
        const ConnectivityStartupConfig& config) = 0;

    /// Selects the final station-only mode when the saved SoftAP is disabled.
    virtual bool finish_station_only_mode() = 0;
};

/// Selects the AP channel and starts required connectivity in normative order.
class ConnectivityStartup {
public:
    /// Returns false for any fatal mode, scan, or concurrent-start failure.
    static bool start(ConnectivityStartupPort& port,
                      std::string_view machine_name,
                      const AccessPointStartupSettings& settings);
};

}  // namespace firmware::application
