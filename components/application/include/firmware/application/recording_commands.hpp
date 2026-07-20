// Declares recording control command handling and exact protocol responses.
#pragma once

#include "firmware/core/frame.hpp"
#include "firmware/core/text.hpp"

namespace firmware::application {

struct RecordingCommandResult {
    bool recognized = false;
    bool requested = false;
    core::Frame response;
};

// Applies M951/M952 state changes and returns the required 0xa2 `ok\n` reply.
RecordingCommandResult handle_recording_command(core::CommandKind command,
                                                bool currently_requested);

}  // namespace firmware::application
