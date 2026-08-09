/** @file @brief Implements preview JSON validation, command matching, and numeric normalization. */
#include "application/playback/preview_request.hpp"
#include "core/filesystem/file_transfer_limits.hpp"

#include "core/configuration/json_input.hpp"

#include <cmath>
#include <limits>

namespace firmware::application {
namespace {

constexpr std::size_t maximum_session_bytes = 39U;

// Maps the case-sensitive wire command to its normalized enum.
std::optional<PreviewCommand> parse_command(std::string_view command) {
    if (command == "open") return PreviewCommand::open;
    if (command == "play") return PreviewCommand::play;
    if (command == "pause") return PreviewCommand::pause;
    if (command == "resume") return PreviewCommand::resume;
    if (command == "seek") return PreviewCommand::seek;
    if (command == "stop") return PreviewCommand::stop;
    return std::nullopt;
}

// Converts an optional finite number to a truncated signed selector or default.
std::int64_t signed_selector(const std::optional<double>& value,
                             std::int64_t default_value) {
    if (!value.has_value() || !std::isfinite(*value)) {
        return default_value;
    }
    const double truncated = std::trunc(*value);
    if (truncated < static_cast<double>(std::numeric_limits<std::int64_t>::min()) ||
        truncated > static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
        return default_value;
    }
    return static_cast<std::int64_t>(truncated);
}

// Converts an optional nonnegative finite number to an unsigned selector or default.
std::uint64_t unsigned_selector(const std::optional<double>& value,
                                std::uint64_t default_value) {
    if (!value.has_value() || !std::isfinite(*value) || *value < 0.0) {
        return default_value;
    }
    const double truncated = std::trunc(*value);
    if (truncated > static_cast<double>(std::numeric_limits<std::uint64_t>::max())) {
        return default_value;
    }
    return static_cast<std::uint64_t>(truncated);
}

// Retains at most the documented number of UTF-8 bytes without decoding them.
std::string bounded_text(const std::optional<std::string>& value,
                         std::size_t maximum_bytes) {
    if (!value.has_value()) {
        return {};
    }
    return value->substr(0U, maximum_bytes);
}

}  // namespace

std::optional<PreviewRequest> parse_preview_request(core::BytesView input) {
    const auto document = core::parse_json_prefix(input);
    if (!document.has_value()) {
        return std::nullopt;
    }
    const auto namespace_name = core::find_json_string(*document, "ns");
    const auto command_name = core::find_json_string(*document, "cmd");
    if (!namespace_name.has_value() || *namespace_name != "vpreview" ||
        !command_name.has_value()) {
        return std::nullopt;
    }
    const auto command = parse_command(*command_name);
    if (!command.has_value()) {
        return std::nullopt;
    }
    PreviewRequest request{};
    request.command = *command;
    request.sequence = static_cast<std::uint32_t>(unsigned_selector(
        core::find_json_number(*document, "seq"), 0U));
    request.path = bounded_text(core::find_json_string(*document, "path"),
                                core::file_transfer_limits::maximum_path_size);
    request.session_id = bounded_text(
        core::find_json_string(*document, "session_id"), maximum_session_bytes);
    request.from_milliseconds = unsigned_selector(
        core::find_json_number(*document, "from_ms"), 0U);
    request.from_frame = signed_selector(
        core::find_json_number(*document, "from_frame"), -1);
    request.time_milliseconds = unsigned_selector(
        core::find_json_number(*document, "t_ms"), 0U);
    request.frame = signed_selector(core::find_json_number(*document, "frame"), -1);
    return request;
}

}  // namespace firmware::application
