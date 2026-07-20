// Implements classic-CAN frame I/O using the ESP-IDF legacy TWAI driver API.
#include "can_twai_adapter.hpp"

#include "firmware/application/can_twai_config.hpp"
#include "firmware/core/can_error_policy.hpp"

#include "driver/twai.h"
#include "freertos/FreeRTOS.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace firmware::target {
namespace {

constexpr int twai_controller_id = 0;
constexpr TickType_t nonblocking_wait = 0U;
constexpr std::uint8_t classic_can_payload_capacity = 8U;
constexpr std::uint16_t maximum_standard_identifier = 0x07ffU;

}  // namespace

bool CanTwaiAdapter::initialize() {
    twai_general_config_t general = TWAI_GENERAL_CONFIG_DEFAULT_V2(
        twai_controller_id,
        static_cast<gpio_num_t>(application::can_twai.tx_gpio),
        static_cast<gpio_num_t>(application::can_twai.rx_gpio),
        TWAI_MODE_NORMAL);
    general.clkout_io =
        static_cast<gpio_num_t>(application::can_twai.clkout_gpio);
    general.bus_off_io = static_cast<gpio_num_t>(
        application::can_twai.bus_off_indicator_gpio);
    general.tx_queue_len = application::can_twai.transmit_queue_capacity;
    general.rx_queue_len = application::can_twai.receive_queue_capacity;

    const twai_timing_config_t timing{
        .clk_src = TWAI_CLK_SRC_DEFAULT,
        .quanta_resolution_hz = application::can_twai.time_quanta_hz,
        .brp = application::can_twai.baud_rate_prescaler,
        .tseg_1 = application::can_twai.time_segment_1,
        .tseg_2 = application::can_twai.time_segment_2,
        .sjw = application::can_twai.synchronization_jump_width,
        .triple_sampling = application::can_twai.triple_sampling,
    };
    const twai_filter_config_t filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&general, &timing, &filter) != ESP_OK) {
        return false;
    }
    if (twai_start() == ESP_OK) {
        return true;
    }
    twai_driver_uninstall();
    return false;
}

bool CanTwaiAdapter::receive(core::CanFrame& frame) const {
    twai_message_t message{};
    if (twai_receive(&message, nonblocking_wait) != ESP_OK ||
        message.extd != 0U || message.rtr != 0U ||
        message.data_length_code > classic_can_payload_capacity) {
        return false;
    }

    frame = {};
    frame.identifier = static_cast<std::uint16_t>(message.identifier);
    frame.size = message.data_length_code;
    std::copy_n(message.data, frame.size, frame.data.begin());
    return true;
}

bool CanTwaiAdapter::transmit(const core::CanFrame& frame) const {
    if (frame.identifier > maximum_standard_identifier ||
        frame.size > classic_can_payload_capacity) {
        return false;
    }
    twai_message_t message{};
    message.identifier = frame.identifier;
    message.data_length_code = frame.size;
    std::copy_n(frame.data.begin(), frame.size, message.data);
    return twai_transmit(&message, nonblocking_wait) == ESP_OK;
}

std::uint8_t CanTwaiAdapter::error_register() const {
    twai_status_info_t status{};
    if (twai_get_status_info(&status) != ESP_OK) {
        return firmware::core::can_error_register_from_status(true, 0U, 0U);
    }
    return firmware::core::can_error_register_from_status(
        status.state == TWAI_STATE_BUS_OFF,
        static_cast<std::uint8_t>(status.tx_error_counter),
        static_cast<std::uint8_t>(status.rx_error_counter));
}

void CanTwaiAdapter::shutdown() const {
    twai_stop();
    twai_driver_uninstall();
}

}  // namespace firmware::target
