/** @file @brief USB physical and protocol activity state transitions. */
#pragma once

#include "application/usb/usb_receive_staging.hpp"
#include "application/usb/usb_transmit_queue.hpp"

#include <atomic>

namespace firmware::application {

/** Coordinates callback/task USB state without depending on TinyUSB types. */
class UsbProtocolState {
public:
    /// Records enumeration without enabling response routing prematurely.
    void enumerated();

    /// Activates protocol routing after the first structurally valid frame.
    void valid_frame_received();

    /** Clears partial receive/activity state while retaining queued responses. */
    void disconnected();

    /// Reports whether both physical presence and protocol activation permit send.
    bool can_send() const;

    /// Reports physical presence independently from protocol activation.
    bool physically_present() const;

    /// Reports whether a valid frame has activated protocol destinations.
    bool protocol_active() const;

    /// Exposes receive staging owned for the full protocol-state lifetime.
    UsbReceiveStaging& receive_staging();
    /// Exposes the queue retained across disconnect/re-enumeration.
    UsbTransmitQueue& transmit_queue();

private:
    /// Callback/task shared flags use lock-free atomic state transitions.
    std::atomic_bool physically_present_{false};
    std::atomic_bool protocol_active_{false};
    UsbReceiveStaging receive_staging_;
    UsbTransmitQueue transmit_queue_;
};

}  // namespace firmware::application
