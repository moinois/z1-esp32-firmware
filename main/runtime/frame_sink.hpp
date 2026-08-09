/** @file @brief Declares transport-specific delivery behind a shared target frame sink. */
#pragma once

#include "core/protocol/frame.hpp"

namespace firmware::target {

/** Minimal encoded-frame destination shared by controller and host adapters. */
class FrameSink {
public:
    virtual ~FrameSink() = default;
    virtual bool send_frame(firmware::core::Frame frame) = 0;
};

}  // namespace firmware::target
