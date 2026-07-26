// Declares the NVS-backed runtime command port for controller UART requests.
#pragma once

#include "firmware/application/runtime_commands.hpp"

namespace firmware::target {

class ControllerUartAdapter;

// Routes runtime responses as framed messages on the controller UART.
class ControllerRuntimeCommandAdapter final
    : public firmware::application::RuntimeCommandPort {
public:
    // Binds persistence and response transport to one controller UART.
    explicit ControllerRuntimeCommandAdapter(ControllerUartAdapter& uart);

    bool admit_operation(std::uint32_t wait_milliseconds) override;
    bool open_namespace(std::string_view name_space) override;
    firmware::application::RuntimeSignedRead read_first_boot(
        std::string_view key) override;
    std::optional<std::uint64_t> read_counter(std::string_view key) override;
    std::optional<std::string> format_utc_minute(
        std::int64_t seconds) override;
    firmware::application::RuntimeEraseResult erase_first_boot(
        std::string_view name_space, std::string_view key) override;
    void complete_operation() override;
    void send_response(std::uint8_t type, std::string_view payload) override;

private:
    ControllerUartAdapter& uart_;
    std::string name_space_;
};

}  // namespace firmware::target
