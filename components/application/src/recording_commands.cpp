// Implements recording command state transitions independent of routing transport.
#include "firmware/application/recording_commands.hpp"

namespace firmware::application {

RecordingCommandResult handle_recording_command(core::CommandKind command,
                                                bool currently_requested) {
    RecordingCommandResult result;
    if (command != core::CommandKind::record_start &&
        command != core::CommandKind::record_stop) {
        return result;
    }
    result.recognized = true;
    result.requested = command == core::CommandKind::record_start;
    (void)currently_requested;
    result.response.type = core::protocol::general_command;
    result.response.payload = {'o', 'k', '\n'};
    return result;
}

}  // namespace firmware::application
