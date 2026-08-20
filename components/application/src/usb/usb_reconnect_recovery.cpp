/** @file @brief Implements callback-safe USB logical reconnect scheduling. */
#include "application/usb/usb_reconnect_recovery.hpp"

namespace firmware::application {

void UsbReconnectRecovery::connection_lost(std::uint64_t now_milliseconds) {
    Phase expected = Phase::inactive;
    if (phase_.compare_exchange_strong(expected, Phase::waiting_to_disconnect,
                                       std::memory_order_acq_rel)) {
        deadline_milliseconds_.store(
            now_milliseconds + usb_reconnect_initial_delay_milliseconds,
            std::memory_order_release);
    }
}

void UsbReconnectRecovery::connection_restored() {
    phase_.store(Phase::inactive, std::memory_order_release);
    deadline_milliseconds_.store(0U, std::memory_order_release);
}

UsbReconnectAction UsbReconnectRecovery::poll(
    std::uint64_t now_milliseconds) {
    const Phase current = phase_.load(std::memory_order_acquire);
    if (current == Phase::inactive ||
        now_milliseconds <
            deadline_milliseconds_.load(std::memory_order_acquire)) {
        return UsbReconnectAction::none;
    }

    if (current == Phase::waiting_to_disconnect ||
        current == Phase::waiting_for_mount) {
        phase_.store(Phase::holding_disconnected, std::memory_order_release);
        deadline_milliseconds_.store(
            now_milliseconds + usb_reconnect_low_hold_milliseconds,
            std::memory_order_release);
        return UsbReconnectAction::drive_bvalid_low;
    }

    phase_.store(Phase::waiting_for_mount, std::memory_order_release);
    deadline_milliseconds_.store(
        now_milliseconds + usb_reconnect_retry_delay_milliseconds,
        std::memory_order_release);
    return UsbReconnectAction::drive_bvalid_high;
}

bool UsbReconnectRecovery::pending() const {
    return phase_.load(std::memory_order_acquire) != Phase::inactive;
}

}  // namespace firmware::application
