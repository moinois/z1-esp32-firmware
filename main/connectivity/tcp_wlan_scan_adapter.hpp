/** @file @brief Declares an ESP-IDF scan port with responses routed to one TCP session. */
#pragma once

#include "application/connectivity/wlan_command.hpp"

namespace firmware::application {
class TcpClientSession;
}

namespace firmware::target {

/** Adapts the shared WLAN scan service to origin-aware TCP responses. */
class TcpWlanScanAdapter final
    : public firmware::application::WlanCommandPort {
public:
    /// Binds Wi-Fi scan results and response frames to one TCP client.
    explicit TcpWlanScanAdapter(firmware::application::TcpClientSession& session);

    void stop_scan() override;
    void delay_milliseconds(std::uint32_t duration) override;
    firmware::application::WifiScanOutcome scan(
        const firmware::application::WifiScanConfig& config) override;
    std::string connected_ssid() const override;
    void send(firmware::core::Frame frame) override;

private:
    firmware::application::TcpClientSession& session_;
};

}  // namespace firmware::target
