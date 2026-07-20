// Implements directory filtering, wire formatting, bounded chunks, and completion.
#include "firmware/application/directory_listing.hpp"

#include "firmware/core/filesystem_syntax.hpp"
#include "firmware/core/protocol_constants.hpp"

#include <cstdio>
#include <string>
#include <utility>

namespace firmware::application {
namespace {

constexpr std::size_t line_size_limit = 256U;
constexpr std::size_t preferred_chunk_flush_size = 411U;
constexpr std::size_t maximum_chunk_payload_size = 535U;
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
core::ByteVector format_line(const DirectoryEntry& entry, bool include_details) {
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
        return {};
    }
    return line;
}

// Sends a nonempty listing chunk and clears its retained storage.
void flush_chunk(core::ByteVector& chunk, DirectoryListPort& port) {
    if (chunk.empty()) {
        return;
    }
    port.send({core::protocol::text_response, std::move(chunk)});
    chunk.clear();
}

// Sends the mandatory successful terminal response for every listing attempt.
void send_completion(DirectoryListPort& port) {
    port.send({core::protocol::operation_success,
               {completion_message.begin(), completion_message.end()}});
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
    chunk.reserve(maximum_chunk_payload_size);
    for (const DirectoryEntry& entry : *entries) {
        core::ByteVector line = format_line(entry, parsed->include_details);
        if (line.empty()) {
            continue;
        }
        if (!chunk.empty() && chunk.size() + line.size() > maximum_chunk_payload_size) {
            flush_chunk(chunk, port);
        }
        chunk.insert(chunk.end(), line.begin(), line.end());
        if (chunk.size() > preferred_chunk_flush_size) {
            flush_chunk(chunk, port);
        }
    }
    flush_chunk(chunk, port);
    send_completion(port);
}

}  // namespace firmware::application
