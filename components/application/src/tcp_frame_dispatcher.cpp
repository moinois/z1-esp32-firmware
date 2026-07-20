// Implements callback-based TCP routing without transport dependencies.
#include "firmware/application/tcp_frame_dispatcher.hpp"

#include <utility>

namespace firmware::application {

TcpFrameDispatcher::TcpFrameDispatcher(Router& router, TcpDispatchSinks sinks)
    : router_(router), sinks_(std::move(sinks)) {}

void TcpFrameDispatcher::dispatch(const HostIdentity& host,
                                  const core::Frame& frame) const {
    const RouteDecision decision = router_.from_host(host, frame);
    if (decision.has(RouteTarget::controller) && sinks_.controller) {
        sinks_.controller(host, frame);
    }
    if (decision.has(RouteTarget::local_command) && sinks_.local_command) {
        sinks_.local_command(host, frame);
    }
    if (decision.has(RouteTarget::file_transfer) && sinks_.file_transfer) {
        sinks_.file_transfer(host, frame);
    }
    if (decision.has(RouteTarget::play_status) && sinks_.play_status) {
        sinks_.play_status(host, frame);
    }
}

}  // namespace firmware::application
