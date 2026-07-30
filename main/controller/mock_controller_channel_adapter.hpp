// Declares a deterministic protocol-level controller UART simulation.
#pragma once

#include "controller_channel_adapter.hpp"
#include "firmware/core/frame.hpp"

#include <deque>

namespace firmware::target {

// Converts normal mainboard queries into deterministic encoded controller replies.
class MockControllerChannelAdapter final : public ControllerChannelAdapter {
public:
    // Initializes the decoder and queues controller identity once.
    bool initialize() override;

    // Returns queued response bytes with the normal UART read bound.
    int read(std::uint8_t* destination, std::size_t capacity) override;

    // Accepts one encoded frame and generates any corresponding mock reply.
    int write(firmware::core::BytesView frame) override;

private:
    // Queues one response using the production frame encoder.
    void queue_response(firmware::core::Frame frame);

    firmware::core::StreamDecoder decoder_{
        firmware::core::StreamPolicy::controller_uart()};
    std::deque<std::uint8_t> pending_input_;
    bool initialized_ = false;
};

}  // namespace firmware::target
