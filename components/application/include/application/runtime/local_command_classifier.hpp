/** @file @brief Classification of locally handled text-command families. */
#pragma once

#include "core/protocol/text.hpp"

namespace firmware::application {

/** Service family responsible for a recognized local command. */
enum class LocalCommandFamily {
    none,
    runtime,
    serial_number,
    recording,
    filesystem,
    wlan,
    access_point,
};

/// Classifies a recognized command without executing or responding to it.
LocalCommandFamily classify_local_command(core::CommandKind command);

}  // namespace firmware::application
