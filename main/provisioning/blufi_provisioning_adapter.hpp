/** @file @brief Declares ESP-IDF Wi-Fi and BLUFI operations for provisioning policy. */
#pragma once

#include "firmware/application/ble_provisioning.hpp"

namespace firmware::target {

/// Translates provisioning port calls into ESP-IDF Wi-Fi/BLUFI operations.
class BlufiProvisioningAdapter final
    : public firmware::application::BleProvisioningPort {
public:
    bool initialize(const firmware::application::BleLifecycleConfig& config) override;
    bool start_advertising(std::string_view device_name) override;
    void stop_advertising() override;
    void create_security_context() override;
    void destroy_security_context() override;
    firmware::application::StationApiResult apply_station_config(
        const firmware::application::StationConfiguration& configuration) override;
    firmware::application::StationApiResult request_station_disconnect() override;
    firmware::application::StationApiResult request_station_connect() override;
    std::uint8_t current_wifi_mode() const override;
    void stop_scan() override;
    void delay_milliseconds(std::uint32_t duration) override;
    firmware::application::BleWifiScanOutcome scan(
        const firmware::application::WifiScanConfig& config) override;
    void report_error(std::uint8_t error) override;
    void send_wifi_status(
        const firmware::application::BleWifiStatusReport& report) override;
    void send_wifi_list(
        const std::vector<firmware::application::BleWifiListEntry>& entries) override;
    void log_custom_data(firmware::core::BytesView data) override;
};

}  // namespace firmware::target
