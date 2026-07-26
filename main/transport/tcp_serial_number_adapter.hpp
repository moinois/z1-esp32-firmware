// Declares shared NVS serial-number behavior routed to one TCP session.
#pragma once

#include "nvs_command_ports.hpp"

namespace firmware::application { class TcpClientSession; }

namespace firmware::target {

class TcpSerialNumberAdapter final : public NvsSerialNumberPort,
                                     private FrameSink {
public:
    explicit TcpSerialNumberAdapter(firmware::application::TcpClientSession& session);

private:
    bool send_frame(firmware::core::Frame frame) override;
    firmware::application::TcpClientSession& session_;
};

}  // namespace firmware::target
