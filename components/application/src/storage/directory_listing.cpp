/** @file @brief Implements directory filtering, wire formatting, bounded chunks, and completion. */
#include "application/storage/directory_listing.hpp"

#include "core/filesystem/filesystem_syntax.hpp"
#include "core/protocol/protocol_constants.hpp"

#include <cstdio>
#include <string>
#include <utility>

namespace firmware::application {
namespace {

constexpr std::size_t line_size_limit = 256U;
constexpr std::size_t preferred_chunk_flush_size = 411U;
constexpr std::size_t maximum_supported_chunk_payload_size = 514U;
constexpr std::size_t timestamp_text_size = 14U;
constexpr std::uint8_t encoded_space = 0x01U;
constexpr std::string_view completion_message = "Load directory finished.\r\n";

// Reports whether a hidden entry is one of the two visible cache directories.
bool visible_name(std::string_view name) {
    return !name.empty() &&
           (name.front() != '.' || name == ".md5" || name == ".lz");
}

// Encodes spaces in a returned filesystem name for the host wire protocol.
core::ByteVector encode_name(std::string_view name) {
    core::ByteVector encoded;
    encoded.reserve(name.size());
    for (const char character : name) {
        encoded.push_back(character == ' '
                              ? encoded_space
                              : static_cast<std::uint8_t>(character));
    }
    return encoded;
}

// Formats a fixed-width UTC timestamp without relying on target locale.
std::string format_timestamp(const UtcFileTime& time) {
    char timestamp[timestamp_text_size + 1U];
    const int length = std::snprintf(timestamp, sizeof(timestamp),
                                     "%04u%02u%02u%02u%02u%02u",
                                     static_cast<unsigned>(time.year),
                                     static_cast<unsigned>(time.month),
                                     static_cast<unsigned>(time.day),
                                     static_cast<unsigned>(time.hour),
                                     static_cast<unsigned>(time.minute),
                                     static_cast<unsigned>(time.second));
    if (length != static_cast<int>(timestamp_text_size)) {
        return {};
    }
    return {timestamp, timestamp_text_size};
}

// Creates one entry line or rejects an entry that cannot be represented.
core::ByteVector format_line(const DirectoryEntry& entry, bool include_details,
                             DirectoryListPort& port) {
    if (!entry.metadata_available || !visible_name(entry.name)) {
        return {};
    }

    core::ByteVector line = encode_name(entry.name);
    if (entry.directory) {
        line.push_back('/');
    }
    if (include_details) {
        const std::uint32_t size =
            entry.directory ? 0U : static_cast<std::uint32_t>(entry.size);
        const std::string timestamp = format_timestamp(entry.modified);
        if (timestamp.empty()) {
            return {};
        }
        const std::string metadata =
            " " + std::to_string(size) + " " + timestamp;
        line.insert(line.end(), metadata.begin(), metadata.end());
    }
    line.push_back('\r');
    line.push_back('\n');
    if (line.size() >= line_size_limit) {
        port.log_warning("Output string too long, truncated");
        return {};
    }
    return line;
}

// Sends a nonempty listing chunk and clears its retained storage.
void flush_chunk(core::ByteVector& chunk, DirectoryListPort& port,
                 std::string_view failure_message) {
    if (chunk.empty()) {
        return;
    }
    if (!port.send({core::protocol::text_response, std::move(chunk)})) {
        port.log_warning(failure_message);
    }
    chunk.clear();
}

// Sends the mandatory successful terminal response for every listing attempt.
void send_completion(DirectoryListPort& port) {
    if (!port.send({core::protocol::operation_success,
                    {completion_message.begin(), completion_message.end()}})) {
        port.log_warning("ls: xRx2ControllerQueue send timeout (LOAD_FINISH)");
    }
}

}  // namespace

void DirectoryListing::execute(core::BytesView argument, DirectoryListPort& port) {
    const auto parsed = core::parse_directory_list_arguments(argument);
    if (!parsed.has_value()) {
        send_completion(port);
        return;
    }
    const auto entries = port.list_directory(parsed->path);
    if (!entries.has_value()) {
        send_completion(port);
        return;
    }

    core::ByteVector chunk;
    chunk.reserve(maximum_supported_chunk_payload_size);
    for (const DirectoryEntry& entry : *entries) {
        const std::size_t path_storage = parsed->path.size() + 1U +
                                         entry.name.size() + 1U;
        if (!port.response_memory_available(path_storage)) {
            port.log_warning(
                "ls: xRx2ControllerQueue send timeout (malloc err path)");
            continue;
        }
        const std::string entry_path = parsed->path + "/" + entry.name;
        if (entry_path.size() >= line_size_limit) {
            port.log_warning("Path too long, truncated: " + entry_path);
            continue;
        }
        core::ByteVector line = format_line(entry, parsed->include_details, port);
        if (line.empty()) {
            continue;
        }
        chunk.insert(chunk.end(), line.begin(), line.end());
        if (chunk.size() > preferred_chunk_flush_size) {
            flush_chunk(chunk, port,
                        "ls: xRx2ControllerQueue send timeout, drop partial chunk");
        }
    }
    flush_chunk(chunk, port,
                "ls: xRx2ControllerQueue send timeout (tail LOAD_INFO)");
    send_completion(port);
}

}  // namespace firmware::application
