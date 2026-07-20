// Implements event-driven station discovery state synchronization.
#include "wlan_event_adapter.hpp"

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include "tcp_discovery_adapter.hpp"
#include "tcp_control_adapter.hpp"
#include "automatic_connection_adapter.hpp"

#include <lwip/inet.h>
#include <lwip/sockets.h>

namespace firmware::target {
namespace {

void station_event_handler(void* context, esp_event_base_t base, int32_t id, void*) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        clear_tcp_discovery_station();
        auto* adapter = static_cast<WlanEventAdapter*>(context);
        if (adapter->automatic_connection() != nullptr) {
            adapter->automatic_connection()->on_station_disconnected();
        }
        return;
    }
    if (base != IP_EVENT || id != IP_EVENT_STA_GOT_IP) {
        return;
    }
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == nullptr) {
        return;
    }
    esp_netif_ip_info_t ip_info{};
    if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) {
        return;
    }
    char address[INET_ADDRSTRLEN]{};
    char netmask[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &ip_info.ip.addr, address, sizeof(address));
    inet_ntop(AF_INET, &ip_info.netmask.addr, netmask, sizeof(netmask));
    update_tcp_discovery_station(address, netmask, active_tcp_client_count());
}

}  // namespace

void WlanEventAdapter::set_automatic_connection(
    AutomaticConnectionAdapter* adapter) {
    automatic_connection_ = adapter;
}

AutomaticConnectionAdapter* WlanEventAdapter::automatic_connection() const {
    return automatic_connection_;
}

void WlanEventAdapter::start() {
    static_cast<void>(esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &station_event_handler, this));
    static_cast<void>(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &station_event_handler, this));
}

}  // namespace firmware::target
