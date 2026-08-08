// Declares the ESP-IDF GPIO and FreeRTOS heartbeat port.
#pragma once

#include "firmware/application/heartbeat.hpp"

namespace firmware::target {

class EspHeartbeatAdapter final : public application::HeartbeatPort {
public:
    bool configure_output() override;
    void set_level(bool high) override;
    void delay_milliseconds(std::uint32_t duration) override;
};

}  // namespace firmware::target
