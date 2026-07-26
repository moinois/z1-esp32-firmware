// Declares shared NVS runtime behavior routed to controller UART.
#pragma once

#include "nvs_command_ports.hpp"

namespace firmware::target {

class ControllerUartAdapter;

class ControllerRuntimeCommandAdapter final : public NvsRuntimeCommandPort,
                                              private FrameSink {
public:
    explicit ControllerRuntimeCommandAdapter(ControllerUartAdapter& uart);

private:
    bool send_frame(firmware::core::Frame frame) override;
    ControllerUartAdapter& uart_;
};

}  // namespace firmware::target
