// Implements event-driven station discovery state synchronization.
#include "wlan_event_adapter.hpp"

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include "tcp_discovery_adapter.hpp"
#include "tcp_control_adapter.hpp"
#include "automatic_connection_adapter.hpp"
#include "firmware/application/ble_provisioning.hpp"
#include "wifi_diagnostic_log.hpp"

#include <lwip/inet.h>
#include <lwip/sockets.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <string>

namespace firmware::target {
namespace {

std::atomic<std::uint8_t> latest_disconnect_reason{0U};

// Records the DHCP client state without exposing ESP-NETIF details to policy.
void log_dhcp_status() {
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == nullptr) {
        static_cast<void>(wifi_diagnostic_log().append(
            "dhcp.status=netif_missing"));
        return;
    }
    esp_netif_dhcp_status_t status = ESP_NETIF_DHCP_INIT;
    const esp_err_t result = esp_netif_dhcpc_get_status(netif, &status);
    if (result != ESP_OK) {
        static_cast<void>(wifi_diagnostic_log().append(
            "dhcp.status.read_error=" + std::to_string(result)));
        return;
    }
    static_cast<void>(wifi_diagnostic_log().trace(
        "dhcp.status=" + std::to_string(static_cast<int>(status))));
}

void station_event_handler(void* context, esp_event_base_t base, int32_t id,
                           void* event_data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        static_cast<void>(wifi_diagnostic_log().trace("wifi.event.station_start"));
    }
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_CONNECTED) {
        static_cast<void>(wifi_diagnostic_log().trace(
            "wifi.event.station_connected"));
        wifi_ap_record_t station_record{};
        wifi_config_t access_point_config{};
        if (esp_wifi_sta_get_ap_info(&station_record) == ESP_OK &&
            esp_wifi_get_config(WIFI_IF_AP, &access_point_config) == ESP_OK) {
            // APSTA requires both interfaces to use the station's channel.
            if (access_point_config.ap.channel != station_record.primary) {
                access_point_config.ap.channel = station_record.primary;
                const esp_err_t channel_result =
                    esp_wifi_set_config(WIFI_IF_AP, &access_point_config);
                static_cast<void>(wifi_diagnostic_log().append(
                    channel_result == ESP_OK
                        ? "wifi.ap_channel.synchronized"
                        : "wifi.ap_channel.synchronize_error"));
            } else {
                static_cast<void>(wifi_diagnostic_log().append(
                    "wifi.ap_channel.already_synchronized"));
            }
        } else {
            static_cast<void>(wifi_diagnostic_log().append(
                "wifi.ap_channel.read_error"));
        }
        log_dhcp_status();
    }
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (event_data != nullptr) {
            const auto* disconnected =
                static_cast<const wifi_event_sta_disconnected_t*>(event_data);
            latest_disconnect_reason.store(disconnected->reason,
                                           std::memory_order_relaxed);
            static_cast<void>(wifi_diagnostic_log().trace(
                "wifi.disconnected.reason=" +
                std::to_string(disconnected->reason)));
        } else {
            static_cast<void>(wifi_diagnostic_log().append(
                "wifi.disconnected.reason=unknown"));
        }
        log_dhcp_status();
        clear_tcp_discovery_station();
        auto* adapter = static_cast<WlanEventAdapter*>(context);
        if (adapter->automatic_connection() != nullptr) {
            adapter->automatic_connection()->on_station_disconnected();
        }
        if (adapter->ble_provisioning() != nullptr) {
            adapter->ble_provisioning()->station_disconnected();
        }
        return;
    }
    if (base != IP_EVENT) {
        return;
    }
    if (id == IP_EVENT_STA_LOST_IP) {
        static_cast<void>(wifi_diagnostic_log().append("wifi.event.lost_ip"));
        log_dhcp_status();
        return;
    }
    if (id != IP_EVENT_STA_GOT_IP) return;
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == nullptr) {
        static_cast<void>(wifi_diagnostic_log().append(
            "wifi.event.got_ip netif_missing"));
        return;
    }
    esp_netif_ip_info_t ip_info{};
    if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) {
        static_cast<void>(wifi_diagnostic_log().append(
            "wifi.event.got_ip ip_read_error"));
        return;
    }
    auto* adapter = static_cast<WlanEventAdapter*>(context);
    if (adapter->ble_provisioning() != nullptr) {
        wifi_ap_record_t access_point{};
        if (esp_wifi_sta_get_ap_info(&access_point) == ESP_OK) {
            std::array<std::uint8_t, 6U> bssid{};
            std::copy(std::begin(access_point.bssid), std::end(access_point.bssid),
                      bssid.begin());
            adapter->ble_provisioning()->station_associated(
                bssid, reinterpret_cast<const char*>(access_point.ssid));
        }
    }
    char address[INET_ADDRSTRLEN]{};
    char netmask[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &ip_info.ip.addr, address, sizeof(address));
    inet_ntop(AF_INET, &ip_info.netmask.addr, netmask, sizeof(netmask));
    static_cast<void>(wifi_diagnostic_log().trace(
        std::string("wifi.event.got_ip address=") + address));
    update_tcp_discovery_station(address, netmask, active_tcp_client_count());
    if (adapter->ble_provisioning() != nullptr) {
        adapter->ble_provisioning()->station_address_ready(address);
    }
}

}  // namespace

std::string last_station_disconnect_detail() {
    return "WiFi association failed; ESP-IDF disconnect reason=" +
           std::to_string(latest_disconnect_reason.load(
               std::memory_order_relaxed));
}

void WlanEventAdapter::set_automatic_connection(
    AutomaticConnectionAdapter* adapter) {
    automatic_connection_ = adapter;
}

AutomaticConnectionAdapter* WlanEventAdapter::automatic_connection() const {
    return automatic_connection_;
}

void WlanEventAdapter::set_ble_provisioning(
    firmware::application::BleProvisioning* provisioning) {
    ble_provisioning_ = provisioning;
}

firmware::application::BleProvisioning* WlanEventAdapter::ble_provisioning() const {
    return ble_provisioning_;
}

void WlanEventAdapter::start() {
    static_cast<void>(esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &station_event_handler, this));
    static_cast<void>(esp_event_handler_register(
        IP_EVENT, ESP_EVENT_ANY_ID, &station_event_handler, this));
}

}  // namespace firmware::target
