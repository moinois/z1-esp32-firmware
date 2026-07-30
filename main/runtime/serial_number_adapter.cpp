// Routes shared NVS serial-number behavior as framed UART responses.
#include "serial_number_adapter.hpp"
#include "controller_channel_adapter.hpp"
#include "esp_log.h"
#include "firmware/core/frame.hpp"

namespace firmware::target {

NvsSerialNumberAdapter::NvsSerialNumberAdapter(ControllerChannelAdapter* channel)
    : NvsSerialNumberPort(static_cast<FrameSink&>(*this)), channel_(channel) {}

bool NvsSerialNumberAdapter::send_frame(firmware::core::Frame frame) {
    if (channel_ == nullptr) return false;
    const auto encoded = firmware::core::encode_frame(frame);
    if (encoded.empty()) return false;
    const int written = channel_->write(encoded);
    if (written != static_cast<int>(encoded.size())) {
        ESP_LOGE("uart_task", "UART send failed");
        return false;
    }
    return true;
}

}  // namespace firmware::target
