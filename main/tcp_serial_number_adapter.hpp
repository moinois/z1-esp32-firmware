// Declares NVS-backed serial-number responses routed to one TCP session.
#pragma once

#include "firmware/application/serial_number.hpp"

namespace firmware::application {
class TcpClientSession;
}

namespace firmware::target {

class TcpSerialNumberAdapter final
    : public firmware::application::SerialNumberPort {
public:
    // Binds serial-number persistence to the originating TCP session.
    explicit TcpSerialNumberAdapter(firmware::application::TcpClientSession& session);

    bool admit_operation(std::uint32_t wait_milliseconds) override;
    firmware::application::SerialNumberRead read_serial(
        std::string_view name_space, std::string_view key) override;
    bool write_serial(std::string_view name_space, std::string_view key,
                      std::string_view value) override;
    void complete_operation() override;
    void send_response(std::uint8_t type, std::string_view payload) override;

private:
    firmware::application::TcpClientSession& session_;
};

}  // namespace firmware::target
