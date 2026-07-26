// Declares shared NVS serial-number behavior routed to controller UART.
#pragma once

#include "nvs_command_ports.hpp"

namespace firmware::target {

class ControllerUartAdapter;

class NvsSerialNumberAdapter final : public NvsSerialNumberPort,
                                     private FrameSink {
public:
    explicit NvsSerialNumberAdapter(ControllerUartAdapter* uart = nullptr);

private:
    bool send_frame(firmware::core::Frame frame) override;
    ControllerUartAdapter* uart_;
};

}  // namespace firmware::target
