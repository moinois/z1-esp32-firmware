/** @file @brief Implements streamed-play line observation, truncation, and long-line collapse. */
#include "application/playback/play_line_reader.hpp"

#include <algorithm>
#include <utility>

namespace firmware::application {
namespace {

constexpr std::size_t maximum_chunk_size = 129U;
constexpr std::size_t maximum_returned_line_size = 64U;

// Returns observed bytes through the first embedded NUL.
core::ByteVector observed_bytes(const core::ByteVector& raw) {
    const auto nul = std::find(raw.begin(), raw.end(), 0U);
    return {raw.begin(), nul};
}

// Reports whether the consumed raw chunk completed a logical line.
bool completes_line(const PlayLineChunk& chunk) {
    return chunk.end_of_file ||
           std::find(chunk.bytes.begin(), chunk.bytes.end(), '\n') != chunk.bytes.end();
}

}  // namespace

PlayLineResult PlayLineReader::read(PlayLineSource& source) {
    auto first = source.read_chunk(maximum_chunk_size);
    if (!first.has_value()) {
        return {PlayLineStatus::failure, {}, false, 0U};
    }
    while (!first->bytes.empty() && first->bytes.front() == 0U) {
        first = source.read_chunk(maximum_chunk_size);
        if (!first.has_value()) {
            return {PlayLineStatus::failure, {}, false, 0U};
        }
    }
    core::ByteVector observed = observed_bytes(first->bytes);
    if (first->bytes.empty() && first->end_of_file) {
        return {PlayLineStatus::end_of_file, {}, true, 0U};
    }

    std::size_t chunk_count = 1U;
    bool reached_eof = first->end_of_file;
    bool complete = completes_line(*first);
    while (!complete) {
        const auto next = source.read_chunk(maximum_chunk_size);
        if (!next.has_value()) {
            return {PlayLineStatus::failure, {}, false, 0U};
        }
        ++chunk_count;
        reached_eof = next->end_of_file;
        complete = completes_line(*next);
    }

    if (chunk_count > 1U) {
        return {PlayLineStatus::line, {'\n'}, reached_eof, 1U};
    }
    const std::size_t observed_size = observed.size();
    if (observed.size() > maximum_returned_line_size) {
        observed.resize(maximum_returned_line_size);
        observed.back() = '\n';
    }
    return {PlayLineStatus::line, std::move(observed), reached_eof, observed_size};
}

}  // namespace firmware::application
