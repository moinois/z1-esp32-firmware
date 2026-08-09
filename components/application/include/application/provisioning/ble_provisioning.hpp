/** @file @brief Declares BLE provisioning lifecycle and product command policy behind a port. */
#pragma once

#include "application/connectivity/station_connection.hpp"
#include "application/connectivity/wlan_command.hpp"
#include "core/protocol/bytes.hpp"
#include "core/network/network_policy.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace firmware::application {

/// Holds the BLE transport and link-security features selected at initialization.
struct BleLifecycleConfig {
    bool standard_blufi_service = false;
    bool low_energy_only = false;
    bool link_pairing_enabled = true;
    bool security_manager_authentication = true;
};

/// Uses the BLUFI station-state values carried by Wi-Fi status reports.
enum class BleStationReportState : std::uint8_t {
    success = 0U,
    failure = 1U,
    connecting = 2U,
};

/// Holds all fields required by one product Wi-Fi status report.
struct BleWifiStatusReport {
    std::uint8_t wifi_mode;
    BleStationReportState station_state;
    std::array<std::uint8_t, 6U> bssid;
    std::string ssid;
};

/// Holds the subset of scan data encoded in a BLUFI Wi-Fi-list record.
struct BleWifiListEntry {
    std::string ssid;
    std::int32_t rssi;
};

/// Distinguishes scan API and result-storage failures from successful results.
struct BleWifiScanOutcome {
    bool success;
    bool result_storage_available;
    std::vector<core::WifiObservation> observations;
};

/// Isolates BLE product policy from BLUFI, security, Wi-Fi, and diagnostics APIs.
class BleProvisioningPort {
public:
    /// Enables safe destruction through a substituted provisioning adapter.
    virtual ~BleProvisioningPort() = default;

    /// Initializes the standard BLUFI service under exact transport policy.
    virtual bool initialize(const BleLifecycleConfig& config) = 0;

    /// Starts connectable advertising under the required device name.
    virtual bool start_advertising(std::string_view device_name) = 0;

    /// Stops advertising when one BLE client connects.
    virtual void stop_advertising() = 0;

    /// Creates a fresh security context for the connected client.
    virtual void create_security_context() = 0;

    /// Destroys the disconnected client's security context.
    virtual void destroy_security_context() = 0;

    /// Applies fast-scan station credentials without manual-connection policy.
    virtual StationApiResult apply_station_config(
        const StationConfiguration& configuration) = 0;

    /// Requests station disconnection without clearing saved credentials.
    virtual StationApiResult request_station_disconnect() = 0;

    /// Requests station connection without a settling delay.
    virtual StationApiResult request_station_connect() = 0;

    /// Returns the adapter's current numeric Wi-Fi mode.
    virtual std::uint8_t current_wifi_mode() const = 0;

    /// Stops any scan that may already be active.
    virtual void stop_scan() = 0;

    /// Waits for the user-scan settling interval.
    virtual void delay_milliseconds(std::uint32_t duration) = 0;

    /// Performs the exact active hidden-network scan used for list requests.
    virtual BleWifiScanOutcome scan(const WifiScanConfig& config) = 0;

    /// Sends one BLUFI error report.
    virtual void report_error(std::uint8_t error) = 0;

    /// Sends one BLUFI Wi-Fi status report.
    virtual void send_wifi_status(const BleWifiStatusReport& report) = 0;

    /// Sends one nonempty BLUFI Wi-Fi list.
    virtual void send_wifi_list(
        const std::vector<BleWifiListEntry>& entries) = 0;

    /// Forwards accepted custom bytes to diagnostic logging.
    virtual void log_custom_data(core::BytesView data) = 0;
};

/// Owns BLE connection identity and implements provisioning product commands.
class BleProvisioning {
public:
    /// Binds provisioning to the shared station runtime and outer adapter.
    BleProvisioning(StationRuntime& runtime, BleProvisioningPort& port,
                    std::string_view machine_name);

    /// Initializes provisioning and starts advertising; failure remains local.
    bool start();

    /// Stops advertising and creates a fresh unready security context.
    void client_connected();

    /// Destroys security and resumes advertising without clearing credentials.
    void client_disconnected();

    /// Marks the current connection's BLUFI security negotiation complete.
    void security_negotiated();

    /// Reports whether product commands may use current connection security.
    bool security_ready() const;

    /// Validates and stages an escaped station SSID after negotiation.
    void receive_ssid(core::BytesView ssid);

    /// Validates and stages an escaped station password after negotiation.
    void receive_password(core::BytesView password);

    /// Starts a secured fast-scan station connection without manual state one.
    void connect_station();

    /// Requests station disconnection without clearing staged credentials.
    void disconnect_station();

    /// Accepts a set-operation-mode request without product effect.
    void set_operation_mode(std::uint8_t mode);

    /// Sends status selected from current mode and shared station runtime.
    void request_status();

    /// Captures association identity for later status and address reports.
    void station_associated(const std::array<std::uint8_t, 6U>& bssid,
                            std::string_view ssid);

    /// Applies address readiness and reports success to a connected BLE client.
    void station_address_ready(std::string_view ipv4);

    /// Clears only association-specific BSSID and address state.
    void station_disconnected();

    /// Performs the specified user scan and sends only a nonempty result list.
    void request_wifi_list();

    /// Echoes an incoming BLUFI error report.
    void receive_error(std::uint8_t error);

    /// Accepts custom data solely for diagnostic logging.
    void receive_custom_data(core::BytesView data);

private:
    /// Builds a status report with the selected station result state.
    BleWifiStatusReport status_report(BleStationReportState state) const;

    StationRuntime& runtime_;
    BleProvisioningPort& port_;
    std::string device_name_;
    std::array<std::uint8_t, 6U> bssid_{};
    std::string associated_ssid_;
    bool client_connected_ = false;
    bool security_ready_ = false;
};

}  // namespace firmware::application
