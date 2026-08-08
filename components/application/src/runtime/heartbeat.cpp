/** @file @brief Implements the portable GPIO heartbeat state and timing policy. */
#include "firmware/application/heartbeat.hpp"

namespace firmware::application {

HeartbeatService::HeartbeatService(HeartbeatPort& port) : port_(port) {}

bool HeartbeatService::start() {
    if (!port_.configure_output()) return false;
    high_ = true;
    started_ = true;
    port_.set_level(high_);
    return true;
}

void HeartbeatService::run_cycle() {
    if (!started_) return;
    port_.delay_milliseconds(period_milliseconds);
    high_ = !high_;
    port_.set_level(high_);
}

}  // namespace firmware::application
