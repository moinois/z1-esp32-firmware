/** @file @brief Implements blocking ESP-IDF Wi-Fi scanning for TCP-origin commands. */
#include "tcp_wlan_scan_adapter.hpp"
#include "esp_wifi_scanner.hpp"

#include "esp_err.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "firmware/application/tcp_client_session.hpp"

#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace firmware::target {

TcpWlanScanAdapter::TcpWlanScanAdapter(
    firmware::application::TcpClientSession& session)
    : session_(session) {}

void TcpWlanScanAdapter::stop_scan() {
    EspWifiScanner{}.stop_scan();
}

void TcpWlanScanAdapter::delay_milliseconds(std::uint32_t duration) {
    vTaskDelay(pdMS_TO_TICKS(duration));
}

firmware::application::WifiScanOutcome TcpWlanScanAdapter::scan(
    const firmware::application::WifiScanConfig& config) {
    return EspWifiScanner{}.scan(config);
}

std::string TcpWlanScanAdapter::connected_ssid() const {
    return EspWifiScanner{}.connected_ssid();
}

void TcpWlanScanAdapter::send(firmware::core::Frame frame) {
    static_cast<void>(session_.queue_frame(frame));
}

}  // namespace firmware::target
