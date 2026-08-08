// Defines normative controller command payloads shared by periodic and update flows.
#pragma once

#include "firmware/core/frame.hpp"
#include "firmware/core/protocol_constants.hpp"

namespace firmware::application {

// Creates the periodic diagnostic command with the LPC-001 line-feed terminator.
inline core::Frame controller_diagnostic_command() {
    return {core::protocol::general_command,
            {'d', 'i', 'a', 'g', 'n', 'o', 's', 'e', '\n'}};
}

// Creates the update reset command with the UPD-005/030/055 line-feed terminator.
inline core::Frame controller_reset_command() {
    return {core::protocol::general_command, {'r', 'e', 's', 'e', 't', '\n'}};
}

}  // namespace firmware::application
