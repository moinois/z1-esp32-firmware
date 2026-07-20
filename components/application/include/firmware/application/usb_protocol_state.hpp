// Declares USB physical/protocol activity and disconnect state transitions.
#pragma once

#include "firmware/application/usb_receive_staging.hpp"
#include "firmware/application/usb_transmit_queue.hpp"

#include <atomic>

namespace firmware::application {

// Coordinates USB activity without coupling policy to TinyUSB callbacks.
class UsbProtocolState {
public:
    // Records physical enumeration without enabling protocol destinations.
    void enumerated();

    // Activates protocol routing after the first structurally valid frame.
    void valid_frame_received();

    // Clears receive state and protocol activity while retaining transmit queue.
    void disconnected();

    // Reports whether responses may currently be sent.
    bool can_send() const;

    // Reports physical presence independently from protocol activity.
    bool physically_present() const;

    // Reports whether a valid frame has activated the protocol.
    bool protocol_active() const;

    UsbReceiveStaging& receive_staging();
    UsbTransmitQueue& transmit_queue();

private:
    // Callback/task shared flags use lock-free atomic state transitions.
    std::atomic_bool physically_present_{false};
    std::atomic_bool protocol_active_{false};
    UsbReceiveStaging receive_staging_;
    UsbTransmitQueue transmit_queue_;
};

}  // namespace firmware::application
