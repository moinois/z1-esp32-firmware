// Implements wall-clock command selection at the target routing boundary.
#include "wall_clock_command_dispatcher.hpp"

#include "firmware/core/protocol_constants.hpp"

namespace firmware::target {

WallClockCommandDispatcher::WallClockCommandDispatcher(
    firmware::application::WallClockPort& port)
    : service_(port) {}

void WallClockCommandDispatcher::dispatch(const firmware::core::Frame& frame) {
    if (frame.type != firmware::core::protocol::general_command) return;
    const std::string_view command(
        reinterpret_cast<const char*>(frame.payload.data()), frame.payload.size());
    if (command == "time" || command.substr(0U, 4U) == "time") {
        service_.handle(command);
    }
}

}  // namespace firmware::target
