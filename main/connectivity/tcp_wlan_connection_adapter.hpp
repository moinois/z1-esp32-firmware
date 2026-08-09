/** @file @brief Declares TCP-origin WLAN connection responses and discovery timing. */
#pragma once

#include "application/connectivity/wlan_command.hpp"

namespace firmware::application {
class TcpClientSession;
}

namespace firmware::target {

/** Routes WLAN connection policy responses back to their originating TCP session. */
class TcpWlanConnectionAdapter final
    : public firmware::application::WlanConnectionResponsePort {
public:
    /// Binds connection responses to one TCP session.
    explicit TcpWlanConnectionAdapter(firmware::application::TcpClientSession& session);

    void send(firmware::core::Frame frame) override;
    void delay_milliseconds(std::uint32_t duration) override;
    void send_discovery_burst() override;

private:
    firmware::application::TcpClientSession& session_;
};

}  // namespace firmware::target
