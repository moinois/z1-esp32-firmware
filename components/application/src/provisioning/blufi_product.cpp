/** @file @brief Implements BLUFI subtype routing and exact product report byte layouts. */
#include "application/provisioning/blufi_product.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace firmware::application {
namespace {

constexpr std::uint8_t acknowledgement_control_subtype = 0U;
constexpr std::uint8_t security_mode_control_subtype = 1U;
constexpr std::uint8_t wifi_mode_control_subtype = 2U;
constexpr std::uint8_t connect_control_subtype = 3U;
constexpr std::uint8_t disconnect_control_subtype = 4U;
constexpr std::uint8_t status_control_subtype = 5U;
constexpr std::uint8_t deauthenticate_control_subtype = 6U;
constexpr std::uint8_t version_control_subtype = 7U;
constexpr std::uint8_t disconnect_ble_control_subtype = 8U;
constexpr std::uint8_t wifi_list_control_subtype = 9U;
constexpr std::uint8_t negotiation_data_subtype = 0U;
constexpr std::uint8_t bssid_data_subtype = 1U;
constexpr std::uint8_t ssid_data_subtype = 2U;
constexpr std::uint8_t password_data_subtype = 3U;
constexpr std::uint8_t first_ignored_configuration_subtype = 4U;
constexpr std::uint8_t last_ignored_configuration_subtype = 0x0EU;
constexpr std::uint8_t version_data_subtype = 0x10U;
constexpr std::uint8_t error_data_subtype = 0x12U;
constexpr std::uint8_t custom_data_subtype = 0x13U;
constexpr std::uint8_t data_format_error = 9U;
constexpr std::uint8_t bssid_tlv_type = 1U;
constexpr std::uint8_t ssid_tlv_type = 2U;
constexpr std::uint8_t bssid_size = 6U;
constexpr std::uint8_t soft_ap_client_count = 0U;
constexpr std::size_t status_fixed_size = 13U;
constexpr std::array<std::uint8_t, 2U> protocol_version{0x01U, 0x03U};

}  // namespace

BlufiProductDispatcher::BlufiProductDispatcher(BlufiProductPort& port)
    : port_(port) {}

void BlufiProductDispatcher::dispatch(const BlufiIncomingFrame& message) {
    if (message.type == BlufiFrameType::control) {
        dispatch_control(message);
    } else if (message.type == BlufiFrameType::data) {
        dispatch_data(message);
    }
}

void BlufiProductDispatcher::dispatch_control(
    const BlufiIncomingFrame& message) {
    switch (message.subtype) {
        case connect_control_subtype:
            port_.connect_station();
            return;
        case disconnect_control_subtype:
            port_.disconnect_station();
            return;
        case status_control_subtype:
            port_.request_wifi_status();
            return;
        case version_control_subtype:
            port_.send_data(
                version_data_subtype,
                core::BytesView(protocol_version.data(),
                                protocol_version.size()));
            return;
        case wifi_list_control_subtype:
            port_.request_wifi_list();
            return;
        case acknowledgement_control_subtype:
        case security_mode_control_subtype:
        case wifi_mode_control_subtype:
        case deauthenticate_control_subtype:
        case disconnect_ble_control_subtype:
        default:
            return;
    }
}

void BlufiProductDispatcher::dispatch_data(
    const BlufiIncomingFrame& message) {
    switch (message.subtype) {
        case negotiation_data_subtype:
            port_.receive_negotiation(message.data);
            return;
        case ssid_data_subtype:
            port_.receive_ssid(message.data);
            return;
        case password_data_subtype:
            port_.receive_password(message.data);
            return;
        case error_data_subtype:
            if (message.data.size() != 1U) {
                port_.report_error(data_format_error);
                return;
            }
            port_.receive_error(message.data[0]);
            return;
        case custom_data_subtype:
            port_.receive_custom_data(message.data);
            return;
        case bssid_data_subtype:
        default:
            if (message.subtype >= first_ignored_configuration_subtype &&
                message.subtype <= last_ignored_configuration_subtype) {
                return;
            }
            return;
    }
}

core::ByteVector encode_blufi_wifi_status(
    const BleWifiStatusReport& report) {
    core::ByteVector data;
    data.reserve(status_fixed_size + report.ssid.size());
    data.push_back(report.wifi_mode);
    data.push_back(static_cast<std::uint8_t>(report.station_state));
    data.push_back(soft_ap_client_count);
    data.push_back(bssid_tlv_type);
    data.push_back(bssid_size);
    data.insert(data.end(), report.bssid.begin(), report.bssid.end());
    data.push_back(ssid_tlv_type);
    data.push_back(static_cast<std::uint8_t>(report.ssid.size()));
    data.insert(data.end(), report.ssid.begin(), report.ssid.end());
    return data;
}

core::ByteVector encode_blufi_wifi_list(
    const std::vector<BleWifiListEntry>& entries) {
    core::ByteVector data;
    for (const BleWifiListEntry& entry : entries) {
        data.push_back(static_cast<std::uint8_t>(entry.ssid.size() + 1U));
        data.push_back(static_cast<std::uint8_t>(entry.rssi));
        data.insert(data.end(), entry.ssid.begin(), entry.ssid.end());
    }
    return data;
}

}  // namespace firmware::application
