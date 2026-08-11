/** @file @brief Declares the process-wide streamed-play session shared by host and controller tasks. */
#pragma once

#include "application/playback/play_session.hpp"
#include "application/playback/play_line_reader.hpp"

#include <optional>
#include <string_view>

namespace firmware::target {

/// Returns the single prepared play session used by all transport adapters.
firmware::application::PlaySession& shared_play_session();
std::optional<std::uint64_t> open_shared_play_file(std::string_view path);
void close_shared_play_file();
bool rewind_shared_play_file();
std::optional<firmware::application::PlayLineChunk> read_shared_play_chunk(
    std::size_t maximum_size);

}  // namespace firmware::target
