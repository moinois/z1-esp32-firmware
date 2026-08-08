/** @file @brief Implements escaped, whitespace-free preview command response JSON. */
#include "firmware/application/preview_responses.hpp"

namespace firmware::application {
namespace {

std::string_view command_name(PreviewCommand command) {
    switch (command) {
        case PreviewCommand::open: return "open";
        case PreviewCommand::play: return "play";
        case PreviewCommand::pause: return "pause";
        case PreviewCommand::resume: return "resume";
        case PreviewCommand::seek: return "seek";
        case PreviewCommand::stop: return "stop";
    }
    return "unknown";
}

void append_string(std::string& output, std::string_view value) {
    output.push_back('"');
    for (const char character : value) {
        if (character == '"' || character == '\\') output.push_back('\\');
        output.push_back(character);
    }
    output.push_back('"');
}

}  // namespace

std::string format_preview_response(PreviewCommand command,
                                    std::uint32_t sequence,
                                    std::int32_t error,
                                    std::string_view session_id,
                                    std::string_view message) {
    std::string output = "{\"ns\":\"vpreview\",\"rsp\":";
    append_string(output, command_name(command));
    output += ",\"seq\":" + std::to_string(sequence) +
              ",\"err\":" + std::to_string(error);
    if (!session_id.empty()) {
        output += ",\"session_id\":";
        append_string(output, session_id);
    }
    if (!message.empty()) {
        output += ",\"message\":";
        append_string(output, message);
    }
    output.push_back('}');
    return output;
}

std::string format_preview_conflict(PreviewCommand command,
                                    std::uint32_t sequence,
                                    std::string_view session_id) {
    return format_preview_response(
        command, sequence, 409, session_id,
        "This conversation is in conflict. Please try again.");
}

}  // namespace firmware::application
