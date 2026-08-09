/** @file @brief Routes shared NVS serial-number behavior to one TCP session. */
#include "tcp_serial_number_adapter.hpp"
#include "application/transport/tcp_client_session.hpp"

namespace firmware::target {

TcpSerialNumberAdapter::TcpSerialNumberAdapter(
    firmware::application::TcpClientSession& session)
    : NvsSerialNumberPort(static_cast<FrameSink&>(*this)), session_(session) {}

bool TcpSerialNumberAdapter::send_frame(firmware::core::Frame frame) {
    return session_.queue_frame(frame);
}

}  // namespace firmware::target
