/** @file @brief Callback dispatch from TCP routing decisions to services. */
#pragma once

#include "application/runtime/router.hpp"
#include "application/transport/tcp_client_session.hpp"

#include <functional>

namespace firmware::application {

/** Optional service sinks corresponding to every TCP-selectable route target. */
struct TcpDispatchSinks {
    std::function<void(TcpClientSession&, const core::Frame&)> controller;
    std::function<void(TcpClientSession&, const core::Frame&)> local_command;
    std::function<void(TcpClientSession&, const core::Frame&)> file_transfer;
    std::function<void(TcpClientSession&, const core::Frame&)> play_status;
};

/** Applies shared routing policy and invokes all selected available sinks. */
class TcpFrameDispatcher {
public:
    /// Creates a dispatcher using the shared ownership-aware router.
    TcpFrameDispatcher(Router& router, TcpDispatchSinks sinks);

    /// Routes one frame to every selected sink; absent sinks are safely ignored.
    void dispatch(TcpClientSession& session, const core::Frame& frame) const;

private:
    Router& router_;
    TcpDispatchSinks sinks_;
};

}  // namespace firmware::application
