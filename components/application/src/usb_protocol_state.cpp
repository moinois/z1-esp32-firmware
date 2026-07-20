// Implements USB enumeration, protocol activation, and disconnect semantics.
#include "firmware/application/usb_protocol_state.hpp"

namespace firmware::application {

void UsbProtocolState::enumerated() {
    physically_present_.store(true, std::memory_order_release);
    protocol_active_.store(false, std::memory_order_release);
}

void UsbProtocolState::valid_frame_received() {
    if (physically_present_.load(std::memory_order_acquire)) {
        protocol_active_.store(true, std::memory_order_release);
    }
}

void UsbProtocolState::disconnected() {
    physically_present_.store(false, std::memory_order_release);
    protocol_active_.store(false, std::memory_order_release);
    receive_staging_.clear();
    // The specification deliberately does not purge the transmit queue here.
}

bool UsbProtocolState::can_send() const {
    return physically_present_.load(std::memory_order_acquire) &&
           protocol_active_.load(std::memory_order_acquire);
}

bool UsbProtocolState::physically_present() const {
    return physically_present_.load(std::memory_order_acquire);
}

bool UsbProtocolState::protocol_active() const {
    return protocol_active_.load(std::memory_order_acquire);
}

UsbReceiveStaging& UsbProtocolState::receive_staging() {
    return receive_staging_;
}

UsbTransmitQueue& UsbProtocolState::transmit_queue() {
    return transmit_queue_;
}

}  // namespace firmware::application
