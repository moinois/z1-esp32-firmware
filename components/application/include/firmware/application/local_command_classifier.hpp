// Declares transport-neutral classification of locally handled text commands.
#pragma once

#include "firmware/core/text.hpp"

namespace firmware::application {

enum class LocalCommandFamily {
    none,
    runtime,
    serial_number,
    recording,
    filesystem,
    wlan,
};

// Classifies one recognized command without executing or responding to it.
LocalCommandFamily classify_local_command(core::CommandKind command);

}  // namespace firmware::application
