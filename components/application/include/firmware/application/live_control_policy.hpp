// Declares transport-independent live-video ownership and preemption policy.
#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace firmware::application {

// Identifies a control action for the live-video execution adapter.
enum class LiveControlAction {
    start,
    stop,
    preempted,
};

// Carries one action and its owning WebSocket identity.
struct LiveControlDecision {
    LiveControlAction action;
    std::uint32_t socket_id;

    // Supports deterministic policy comparisons in host tests.
    bool operator==(const LiveControlDecision& other) const {
        return action == other.action && socket_id == other.socket_id;
    }
};

// Implements exact command matching and single-owner live-stream arbitration.
class LiveControlPolicy {
public:
    // Applies one complete text command from a socket identity.
    std::vector<LiveControlDecision> handle(std::uint32_t socket_id,
                                             std::string_view payload);

    // Stops a stream only when its owning socket disconnects.
    std::vector<LiveControlDecision> on_disconnect(std::uint32_t socket_id);

private:
    std::optional<std::uint32_t> owner_;
};

}  // namespace firmware::application
