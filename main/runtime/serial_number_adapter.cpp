// Routes shared NVS serial-number behavior as framed UART responses.
#include "serial_number_adapter.hpp"
#include "controller_uart_adapter.hpp"
#include "esp_log.h"
#include "firmware/core/frame.hpp"

namespace firmware::target {

NvsSerialNumberAdapter::NvsSerialNumberAdapter(ControllerUartAdapter* uart)
    : NvsSerialNumberPort(static_cast<FrameSink&>(*this)), uart_(uart) {}

bool NvsSerialNumberAdapter::send_frame(firmware::core::Frame frame) {
    if (uart_ == nullptr) return false;
    const auto encoded = firmware::core::encode_frame(frame);
    if (encoded.empty()) return false;
    const int written = uart_->write(encoded);
    if (written != static_cast<int>(encoded.size())) {
        ESP_LOGE("uart_task", "UART send failed");
        return false;
    }
    return true;
}

}  // namespace firmware::target
