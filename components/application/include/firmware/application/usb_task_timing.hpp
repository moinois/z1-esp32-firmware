// Defines the cooperative polling interval shared by USB worker tasks.
#pragma once

#include <cstdint>

namespace firmware::application {

// Leaves enough scheduler time for the idle watchdog while retaining prompt I/O.
inline constexpr std::uint32_t usb_task_poll_delay_milliseconds = 20U;

}  // namespace firmware::application
