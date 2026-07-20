// Implements BLE lifecycle gates and provisioning Wi-Fi command behavior.
#include "firmware/application/ble_provisioning.hpp"

#include "firmware/application/connectivity_defaults.hpp"
#include "firmware/core/text.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace firmware::application {
namespace {

constexpr std::string_view provisioning_device_name = "BLUFI_DEVICE";
constexpr std::size_t maximum_ssid_input_size = 31U;
constexpr std::size_t maximum_password_input_size = 63U;
constexpr std::uint8_t data_format_error = 9U;
constexpr std::uint8_t wifi_scan_error = 11U;
constexpr WifiScanConfig provisioning_scan_config{
    true,
    true,
    connectivity_defaults::user_scan_active_dwell_milliseconds,
    connectivity_defaults::user_scan_passive_dwell_milliseconds,
    connectivity_defaults::user_scan_maximum_observations,
};

constexpr BleLifecycleConfig lifecycle_config{
    true,
    true,
    false,
    false,
};

}  // namespace

BleProvisioning::BleProvisioning(StationRuntime& runtime,
                                 BleProvisioningPort& port)
    : runtime_(runtime), port_(port) {}

bool BleProvisioning::start() {
    if (!port_.initialize(lifecycle_config)) {
        return false;
    }
    return port_.start_advertising(provisioning_device_name);
}

void BleProvisioning::client_connected() {
    port_.stop_advertising();
    port_.create_security_context();
    client_connected_ = true;
    security_ready_ = false;
}

void BleProvisioning::client_disconnected() {
    port_.destroy_security_context();
    client_connected_ = false;
    security_ready_ = false;
    static_cast<void>(port_.start_advertising(provisioning_device_name));
}

void BleProvisioning::security_negotiated() {
    if (client_connected_) {
        security_ready_ = true;
    }
}

bool BleProvisioning::security_ready() const {
    return security_ready_;
}

void BleProvisioning::receive_ssid(core::BytesView ssid) {
    if (!security_ready_) {
        return;
    }
    if (ssid.size() > maximum_ssid_input_size) {
        port_.report_error(data_format_error);
        return;
    }
    runtime_.ssid = core::decode_escaped(ssid);
}

void BleProvisioning::receive_password(core::BytesView password) {
    if (!security_ready_) {
        return;
    }
    if (password.size() > maximum_password_input_size) {
        port_.report_error(data_format_error);
        return;
    }
    runtime_.password = core::decode_escaped(password);
}

void BleProvisioning::connect_station() {
    if (!security_ready_) {
        return;
    }
    if (runtime_.ssid.empty()) {
        port_.report_error(data_format_error);
        return;
    }

    StationConfiguration configuration;
    configuration.ssid = runtime_.ssid;
    configuration.password = runtime_.password;
    configuration.scan_all_channels = false;
    configuration.fast_scan = true;
    if (runtime_.state != StationConnectionState::attempt_started) {
        runtime_.state = StationConnectionState::idle;
    }
    runtime_.ipv4.clear();
    runtime_.has_error = false;
    runtime_.error_detail.clear();
    static_cast<void>(port_.apply_station_config(configuration));
    static_cast<void>(port_.request_station_disconnect());
    static_cast<void>(port_.request_station_connect());
}

void BleProvisioning::disconnect_station() {
    static_cast<void>(port_.request_station_disconnect());
}

void BleProvisioning::set_operation_mode(std::uint8_t mode) {
    static_cast<void>(mode);
}

BleWifiStatusReport BleProvisioning::status_report(
    BleStationReportState state) const {
    return {port_.current_wifi_mode(), state, bssid_, runtime_.ssid};
}

void BleProvisioning::request_status() {
    BleStationReportState state = BleStationReportState::failure;
    if (runtime_.state == StationConnectionState::address_ready &&
        runtime_.error_detail.empty()) {
        state = BleStationReportState::success;
    } else if (runtime_.state == StationConnectionState::attempt_started) {
        state = BleStationReportState::connecting;
    }
    port_.send_wifi_status(status_report(state));
}

void BleProvisioning::station_associated(
    const std::array<std::uint8_t, 6U>& bssid, std::string_view ssid) {
    bssid_ = bssid;
    associated_ssid_ = std::string(ssid);
    StationRuntimeEvents::associated(runtime_, ssid);
}

void BleProvisioning::station_address_ready(std::string_view ipv4) {
    StationRuntimeEvents::address_ready(runtime_, ipv4);
    if (client_connected_) {
        BleWifiStatusReport report =
            status_report(BleStationReportState::success);
        report.ssid = associated_ssid_;
        port_.send_wifi_status(report);
    }
}

void BleProvisioning::station_disconnected() {
    bssid_.fill(0U);
    associated_ssid_.clear();
    runtime_.state = StationConnectionState::idle;
    runtime_.ipv4.clear();
}

void BleProvisioning::request_wifi_list() {
    port_.stop_scan();
    port_.delay_milliseconds(
        connectivity_defaults::user_scan_settle_milliseconds);
    const BleWifiScanOutcome outcome = port_.scan(provisioning_scan_config);
    if (!outcome.success || !outcome.result_storage_available) {
        port_.report_error(wifi_scan_error);
        return;
    }

    const auto results = core::process_wifi_scan(outcome.observations, {});
    if (results.empty()) {
        return;
    }
    std::vector<BleWifiListEntry> entries;
    entries.reserve(results.size());
    for (const core::WifiScanResult& result : results) {
        entries.push_back({result.ssid, result.rssi});
    }
    port_.send_wifi_list(entries);
}

void BleProvisioning::receive_error(std::uint8_t error) {
    port_.report_error(error);
}

void BleProvisioning::receive_custom_data(core::BytesView data) {
    port_.log_custom_data(data);
}

}  // namespace firmware::application
