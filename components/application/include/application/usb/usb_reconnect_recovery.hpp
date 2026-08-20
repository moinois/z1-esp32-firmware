/** @file @brief Portable scheduling policy for recovering a suspended USB link. */
#pragma once

#include <atomic>
#include <cstdint>

namespace firmware::application {

/** Hardware action requested by the USB reconnect recovery scheduler. */
enum class UsbReconnectAction : std::uint8_t {
    none,
    drive_bvalid_low,
    drive_bvalid_high,
};

/**
 * Schedules bounded logical VBUS cycles after TinyUSB loses host activity.
 *
 * The class deliberately knows nothing about ESP-IDF or GPIO routing. TinyUSB
 * callbacks only arm or cancel recovery, while a normal worker task polls and
 * performs the requested hardware action. This avoids delays and register
 * changes inside callbacks owned by the USB stack.
 */
class UsbReconnectRecovery {
public:
    /** Arms recovery after suspend or unmount without delaying the callback. */
    void connection_lost(std::uint64_t now_milliseconds);

    /** Cancels all pending cycles after mount or resume proves host activity. */
    void connection_restored();

    /**
     * Returns the next due BVALID action and advances the retry schedule.
     *
     * @param now_milliseconds Monotonic milliseconds from the target clock.
     */
    UsbReconnectAction poll(std::uint64_t now_milliseconds);

    /** Reports whether a logical reconnect cycle remains pending. */
    bool pending() const;

private:
    enum class Phase : std::uint8_t {
        inactive,
        waiting_to_disconnect,
        holding_disconnected,
        waiting_for_mount,
    };

    std::atomic<Phase> phase_{Phase::inactive};
    std::atomic<std::uint64_t> deadline_milliseconds_{0U};
};

/** Grace period that ignores brief host scheduling or sleep transitions. */
inline constexpr std::uint64_t usb_reconnect_initial_delay_milliseconds = 1000U;

/** Minimum logical VBUS-low interval required by the DWC disconnect path. */
inline constexpr std::uint64_t usb_reconnect_low_hold_milliseconds = 100U;

/** Retry interval used while the physical cable remains absent. */
inline constexpr std::uint64_t usb_reconnect_retry_delay_milliseconds = 1000U;

}  // namespace firmware::application
