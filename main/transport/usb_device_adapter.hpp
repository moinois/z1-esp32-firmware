/// @file
/// Declares the ESP-IDF TinyUSB vendor-interface lifecycle adapter.
#pragma once

#include "firmware/core/frame.hpp"

namespace firmware::target {

/// Owns startup of the board's native USB protocol transport.
///
/// The adapter installs the TinyUSB vendor interface, initializes the shared
/// USB protocol state, and starts the receive, transmit, file-transfer, and
/// local-command workers. Protocol commands are delegated to the same
/// transport-neutral application services used by the other transports.
///
/// This class does not own a USB connection. TinyUSB reports mount and unmount
/// events through its callbacks, and the adapter keeps running so a host may
/// disconnect and reconnect.
class UsbDeviceAdapter {
public:
    /// Installs TinyUSB and starts all native USB worker tasks.
    ///
    /// Call this once during target startup after the services used by the USB
    /// command handlers have been initialized. A failure is logged and
    /// returned to the caller without aborting startup of unrelated services.
    ///
    /// @return `true` when TinyUSB and every worker task were started;
    ///         otherwise `false`.
    bool start();
};

/// Queues a protocol frame for asynchronous transmission over native USB.
///
/// The frame is encoded before it is placed in the bounded transmit queue.
/// This function does not wait for a host to read the encoded bytes.
///
/// @param frame Complete protocol frame to encode and enqueue. The caller
///              retains ownership of the supplied frame.
/// @return `true` when the encoded frame was accepted by the transmit queue;
///         `false` when encoding failed or the queue rejected the frame.
bool queue_usb_frame(const firmware::core::Frame& frame);

}  // namespace firmware::target
