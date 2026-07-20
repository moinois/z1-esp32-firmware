// Implements USB enumeration, protocol activation, and disconnect semantics.
#include "firmware/application/usb_protocol_state.hpp"

namespace firmware::application {

void UsbProtocolState::enumerated() {
    physically_present_ = true;
    protocol_active_ = false;
}

void UsbProtocolState::valid_frame_received() {
    if (physically_present_) {
        protocol_active_ = true;
    }
}

void UsbProtocolState::disconnected() {
    physically_present_ = false;
    protocol_active_ = false;
    receive_staging_.clear();
    // The specification deliberately does not purge the transmit queue here.
}

bool UsbProtocolState::can_send() const {
    return physically_present_ && protocol_active_;
}

bool UsbProtocolState::physically_present() const {
    return physically_present_;
}

bool UsbProtocolState::protocol_active() const {
    return protocol_active_;
}

UsbReceiveStaging& UsbProtocolState::receive_staging() {
    return receive_staging_;
}

UsbTransmitQueue& UsbProtocolState::transmit_queue() {
    return transmit_queue_;
}

}  // namespace firmware::application
