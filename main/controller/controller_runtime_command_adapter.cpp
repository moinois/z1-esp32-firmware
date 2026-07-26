// Routes shared NVS runtime behavior as framed UART responses.
#include "controller_runtime_command_adapter.hpp"
#include "controller_uart_adapter.hpp"
#include "esp_log.h"
#include "firmware/core/frame.hpp"

namespace firmware::target {

ControllerRuntimeCommandAdapter::ControllerRuntimeCommandAdapter(
    ControllerUartAdapter& uart)
    : NvsRuntimeCommandPort(static_cast<FrameSink&>(*this)), uart_(uart) {}

bool ControllerRuntimeCommandAdapter::send_frame(firmware::core::Frame frame) {
    const auto encoded = firmware::core::encode_frame(frame);
    if (encoded.empty()) return false;
    const int written = uart_.write(encoded);
    if (written != static_cast<int>(encoded.size())) {
        ESP_LOGE("uart_task", "UART send failed");
        return false;
    }
    return true;
}

}  // namespace firmware::target
