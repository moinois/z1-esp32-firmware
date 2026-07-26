// Declares transport-specific delivery behind a shared target frame sink.
#pragma once

#include "firmware/core/frame.hpp"

namespace firmware::target {

class FrameSink {
public:
    virtual ~FrameSink() = default;
    virtual bool send_frame(firmware::core::Frame frame) = 0;
};

}  // namespace firmware::target
