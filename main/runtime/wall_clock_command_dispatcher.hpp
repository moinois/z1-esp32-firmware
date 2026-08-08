/** @file @brief Declares target dispatch of recognized wall-clock command frames. */
#pragma once

#include "firmware/application/wall_clock.hpp"
#include "firmware/core/frame.hpp"

namespace firmware::target {

/** Serializes wall-clock commands and routes replies to the originating transport. */
class WallClockCommandDispatcher {
public:
    /// Binds command dispatch to one concrete wall-clock port.
    explicit WallClockCommandDispatcher(firmware::application::WallClockPort& port);

    /// Handles a general-command frame and ignores unrelated traffic.
    void dispatch(const firmware::core::Frame& frame);

private:
    firmware::application::WallClockService service_;
};

}  // namespace firmware::target
