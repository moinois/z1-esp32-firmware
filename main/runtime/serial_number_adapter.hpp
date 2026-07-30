// Declares shared NVS serial-number behavior routed to controller UART.
#pragma once

#include "nvs_command_ports.hpp"

namespace firmware::target {

class ControllerChannelAdapter;

class NvsSerialNumberAdapter final : public NvsSerialNumberPort,
                                     private FrameSink {
public:
    explicit NvsSerialNumberAdapter(ControllerChannelAdapter* channel = nullptr);

private:
    bool send_frame(firmware::core::Frame frame) override;
    ControllerChannelAdapter* channel_;
};

}  // namespace firmware::target
