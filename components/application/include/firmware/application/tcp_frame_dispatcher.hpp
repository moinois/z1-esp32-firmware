// Declares callback-based dispatch from TCP routing decisions to services.
#pragma once

#include "firmware/application/router.hpp"
#include "firmware/application/tcp_client_session.hpp"

#include <functional>

namespace firmware::application {

struct TcpDispatchSinks {
    std::function<void(TcpClientSession&, const core::Frame&)> controller;
    std::function<void(TcpClientSession&, const core::Frame&)> local_command;
    std::function<void(TcpClientSession&, const core::Frame&)> file_transfer;
    std::function<void(TcpClientSession&, const core::Frame&)> play_status;
};

class TcpFrameDispatcher {
public:
    // Creates a dispatcher that evaluates host frames through the shared router.
    TcpFrameDispatcher(Router& router, TcpDispatchSinks sinks);

    // Routes one frame to every selected sink; absent sinks are safely ignored.
    void dispatch(TcpClientSession& session, const core::Frame& frame) const;

private:
    Router& router_;
    TcpDispatchSinks sinks_;
};

}  // namespace firmware::application
