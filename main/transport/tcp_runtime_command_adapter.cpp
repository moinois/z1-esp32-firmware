/** @file @brief Routes shared NVS runtime behavior to one TCP session. */
#include "tcp_runtime_command_adapter.hpp"
#include "application/transport/tcp_client_session.hpp"

namespace firmware::target {

TcpRuntimeCommandAdapter::TcpRuntimeCommandAdapter(
    firmware::application::TcpClientSession& session)
    : NvsRuntimeCommandPort(static_cast<FrameSink&>(*this)), session_(session) {}

bool TcpRuntimeCommandAdapter::send_frame(firmware::core::Frame frame) {
    return session_.queue_frame(frame);
}

}  // namespace firmware::target
