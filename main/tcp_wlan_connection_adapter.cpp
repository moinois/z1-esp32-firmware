// Implements origin-aware WLAN response transport for TCP clients.
#include "tcp_wlan_connection_adapter.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "firmware/application/tcp_client_session.hpp"

namespace firmware::target {

TcpWlanConnectionAdapter::TcpWlanConnectionAdapter(
    firmware::application::TcpClientSession& session)
    : session_(session) {}

void TcpWlanConnectionAdapter::send(firmware::core::Frame frame) {
    static_cast<void>(session_.queue_frame(frame));
}

void TcpWlanConnectionAdapter::delay_milliseconds(std::uint32_t duration) {
    vTaskDelay(pdMS_TO_TICKS(duration));
}

void TcpWlanConnectionAdapter::send_discovery_burst() {}

}  // namespace firmware::target
