/** @file @brief Target-independent heartbeat startup and toggle policy. */
#pragma once

#include <cstdint>

namespace firmware::application {

/** Physical GPIO and scheduler operations required by the heartbeat service. */
class HeartbeatPort {
public:
    /// Enables safe destruction through a substituted hardware adapter.
    virtual ~HeartbeatPort() = default;
    /// Configures the heartbeat pin; false leaves the service stopped.
    virtual bool configure_output() = 0;
    /// Drives the configured heartbeat output.
    virtual void set_level(bool high) = 0;
    /// Yields the current task for the requested duration.
    virtual void delay_milliseconds(std::uint32_t duration) = 0;
};

/** Produces an initial high level followed by exact one-second inversions. */
class HeartbeatService {
public:
    /// Interval between output inversions.
    static constexpr std::uint32_t period_milliseconds = 1000U;

    /// Binds the lifecycle policy to replaceable GPIO and delay operations.
    explicit HeartbeatService(HeartbeatPort& port);
    /// Configures and drives the initial high level; safe to call once.
    bool start();
    /// Delays one period and toggles only after successful startup.
    void run_cycle();

private:
    HeartbeatPort& port_;
    bool high_ = false;
    bool started_ = false;
};

}  // namespace firmware::application
