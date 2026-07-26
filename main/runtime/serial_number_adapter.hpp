// Declares the target serial-number service adapter over NVS and UART.
#pragma once

#include "firmware/application/serial_number.hpp"

namespace firmware::target {

class ControllerUartAdapter;

class NvsSerialNumberAdapter final : public firmware::application::SerialNumberPort {
public:
    // Binds serial-number persistence and responses to target services.
    explicit NvsSerialNumberAdapter(ControllerUartAdapter* uart = nullptr);

    bool admit_operation(std::uint32_t wait_milliseconds) override;
    firmware::application::SerialNumberRead read_serial(
        std::string_view name_space, std::string_view key) override;
    bool write_serial(std::string_view name_space, std::string_view key,
                      std::string_view value) override;
    void complete_operation() override;
    void send_response(std::uint8_t type, std::string_view payload) override;

private:
    ControllerUartAdapter* uart_ = nullptr;
};

}  // namespace firmware::target
