/** @file @brief Declares shared NVS runtime behavior routed to controller UART. */
#pragma once

#include "nvs_command_ports.hpp"

namespace firmware::target {

class ControllerChannelAdapter;

/** Executes NVS-backed runtime commands originating from the controller UART. */
class ControllerRuntimeCommandAdapter final : public NvsRuntimeCommandPort,
                                              private FrameSink {
public:
    explicit ControllerRuntimeCommandAdapter(ControllerChannelAdapter& channel);

private:
    bool send_frame(firmware::core::Frame frame) override;
    ControllerChannelAdapter& channel_;
};

}  // namespace firmware::target
