/** @file @brief Implements compact ordered media JSON responses with JSON string escaping. */
#include "core/media/media_messages.hpp"

namespace firmware::core {
namespace {

constexpr std::string_view occupied_message =
    "The video stream channel is already occupied.";

// Appends one JSON-escaped string without adding whitespace or a newline.
void append_json_string(std::string& output, std::string_view value) {
    output.push_back('"');
    for (const char character : value) {
        switch (character) {
            case '"': output.append("\\\""); break;
            case '\\': output.append("\\\\"); break;
            case '\b': output.append("\\b"); break;
            case '\f': output.append("\\f"); break;
            case '\n': output.append("\\n"); break;
            case '\r': output.append("\\r"); break;
            case '\t': output.append("\\t"); break;
            default:
                if (static_cast<unsigned char>(character) < 0x20U) {
                    output.append("\\u00");
                    constexpr char hex[] = "0123456789abcdef";
                    output.push_back(hex[(static_cast<unsigned char>(character) >> 4U) & 0x0FU]);
                    output.push_back(hex[static_cast<unsigned char>(character) & 0x0FU]);
                } else {
                    output.push_back(character);
                }
                break;
        }
    }
    output.push_back('"');
}

// Appends the common ordered fields used by both preemption responses.
void append_common_preemption(std::string& output, std::string_view namespace_name,
                              std::string_view reason) {
    output.append("{\"ns\":");
    append_json_string(output, namespace_name);
    output.append(",\"rsp\":\"preempted\",\"reason\":");
    append_json_string(output, reason);
    output.append(",\"message\":");
    append_json_string(output, occupied_message);
}

}  // namespace

std::string format_live_preemption(std::string_view reason) {
    std::string output;
    append_common_preemption(output, "vlive", reason);
    output.push_back('}');
    return output;
}

std::string format_preview_preemption(std::string_view reason,
                                      std::string_view session_id) {
    std::string output;
    append_common_preemption(output, "vpreview", reason);
    if (!session_id.empty()) {
        output.append(",\"session_id\":");
        append_json_string(output, session_id);
    }
    output.push_back('}');
    return output;
}

}  // namespace firmware::core
