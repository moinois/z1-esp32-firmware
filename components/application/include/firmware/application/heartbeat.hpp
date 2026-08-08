// Declares target-independent heartbeat startup and one-second toggle policy.
#pragma once

#include <cstdint>

namespace firmware::application {

class HeartbeatPort {
public:
    virtual ~HeartbeatPort() = default;
    virtual bool configure_output() = 0;
    virtual void set_level(bool high) = 0;
    virtual void delay_milliseconds(std::uint32_t duration) = 0;
};

class HeartbeatService {
public:
    static constexpr std::uint32_t period_milliseconds = 1000U;

    explicit HeartbeatService(HeartbeatPort& port);
    bool start();
    void run_cycle();

private:
    HeartbeatPort& port_;
    bool high_ = false;
    bool started_ = false;
};

}  // namespace firmware::application
