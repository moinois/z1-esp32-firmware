// Declares shared NVS runtime behavior routed to one TCP session.
#pragma once

#include "nvs_command_ports.hpp"

namespace firmware::application { class TcpClientSession; }

namespace firmware::target {

class TcpRuntimeCommandAdapter final : public NvsRuntimeCommandPort,
                                       private FrameSink {
public:
    explicit TcpRuntimeCommandAdapter(firmware::application::TcpClientSession& session);

private:
    bool send_frame(firmware::core::Frame frame) override;
    firmware::application::TcpClientSession& session_;
};

}  // namespace firmware::target
