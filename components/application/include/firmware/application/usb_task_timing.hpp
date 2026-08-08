/** @file @brief Cooperative polling interval shared by USB worker tasks. */
#pragma once

#include <cstdint>

namespace firmware::application {

/// Leaves idle-watchdog scheduler time while retaining responsive USB I/O.
inline constexpr std::uint32_t usb_task_poll_delay_milliseconds = 20U;

}  // namespace firmware::application
