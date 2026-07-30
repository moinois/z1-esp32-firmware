// Implements deterministic controller responses over the production framing codec.
#include "mock_controller_channel_adapter.hpp"

#include "firmware/core/protocol_constants.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <algorithm>
#include <string_view>

namespace firmware::target {
namespace {

constexpr char tag[] = "MOCK_CTRL";
constexpr std::string_view mock_version = "mock-controller-1";
constexpr std::string_view mock_status =
    "<Idle|MPos:0.0000,0.0000,0.0000,0.0000,0.0000|C:0,0,0,0>\n";
constexpr std::string_view mock_diagnostic =
    "{MOCK:1|UART:OK|CTRL:SIMULATED}\n";

// Converts text constants to owned protocol payloads.
firmware::core::ByteVector bytes(std::string_view text) {
    return {text.begin(), text.end()};
}

}  // namespace

bool MockControllerChannelAdapter::initialize() {
    if (initialized_) {
        return true;
    }
    initialized_ = true;
    queue_response({firmware::core::protocol::controller_version,
                    bytes(mock_version)});
    ESP_LOGW(tag, "TEST BUILD: deterministic mock controller initialized");
    return true;
}

int MockControllerChannelAdapter::read(std::uint8_t* destination,
                                       std::size_t capacity) {
    if (!initialized_ || destination == nullptr || capacity == 0U) {
        return 0;
    }
    if (pending_input_.empty()) {
        vTaskDelay(pdMS_TO_TICKS(30U));
        return 0;
    }
    const std::size_t count = std::min(capacity, pending_input_.size());
    for (std::size_t index = 0U; index < count; ++index) {
        destination[index] = pending_input_.front();
        pending_input_.pop_front();
    }
    return static_cast<int>(count);
}

int MockControllerChannelAdapter::write(firmware::core::BytesView frame) {
    if (!initialized_) {
        return -1;
    }
    for (const auto& decoded : decoder_.push(frame)) {
        const std::string_view payload(
            reinterpret_cast<const char*>(decoded.payload.data()),
            decoded.payload.size());
        if (decoded.type == firmware::core::protocol::single_command &&
            payload == "?") {
            queue_response({firmware::core::protocol::machine_status,
                            bytes(mock_status)});
        } else if (decoded.type == firmware::core::protocol::general_command &&
                   payload.starts_with("diagnose")) {
            queue_response({firmware::core::protocol::diagnostic_data,
                            bytes(mock_diagnostic)});
        }
    }
    return static_cast<int>(frame.size());
}

void MockControllerChannelAdapter::queue_response(
    firmware::core::Frame frame) {
    const auto encoded = firmware::core::encode_frame(frame);
    pending_input_.insert(pending_input_.end(), encoded.begin(), encoded.end());
}

}  // namespace firmware::target
