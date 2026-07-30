// Routes shared NVS runtime behavior as framed UART responses.
#include "controller_runtime_command_adapter.hpp"
#include "controller_channel_adapter.hpp"
#include "esp_log.h"
#include "firmware/core/frame.hpp"

namespace firmware::target {

ControllerRuntimeCommandAdapter::ControllerRuntimeCommandAdapter(
    ControllerChannelAdapter& channel)
    : NvsRuntimeCommandPort(static_cast<FrameSink&>(*this)), channel_(channel) {}

bool ControllerRuntimeCommandAdapter::send_frame(firmware::core::Frame frame) {
    const auto encoded = firmware::core::encode_frame(frame);
    if (encoded.empty()) return false;
    const int written = channel_.write(encoded);
    if (written != static_cast<int>(encoded.size())) {
        ESP_LOGE("uart_task", "UART send failed");
        return false;
    }
    return true;
}

}  // namespace firmware::target
