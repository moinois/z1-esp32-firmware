/** @file @brief Owns the process-wide streamed-play session instance. */
#include "play_runtime_state.hpp"
#include "posix_file.hpp"

#include <cstdio>
#include <memory>
#include <utility>
#include <vector>

namespace firmware::target {
namespace {
firmware::application::PlaySession play_session;
std::unique_ptr<PosixFile> active_play_file;
std::vector<std::unique_ptr<PosixFile>> superseded_play_files;
}  // namespace

firmware::application::PlaySession& shared_play_session() {
    return play_session;
}

std::optional<std::uint64_t> open_shared_play_file(std::string_view path) {
    auto candidate = std::make_unique<PosixFile>();
    if (!candidate->open(path, "rb")) return std::nullopt;
    const std::uint64_t observed_size = candidate->size().value_or(0U);
    if (active_play_file != nullptr) {
        superseded_play_files.push_back(std::move(active_play_file));
    }
    active_play_file = std::move(candidate);
    return observed_size;
}

void close_shared_play_file() {
    if (active_play_file != nullptr) active_play_file->close();
    active_play_file.reset();
}

bool rewind_shared_play_file() {
    return active_play_file != nullptr && active_play_file->rewind();
}

std::optional<firmware::application::PlayLineChunk> read_shared_play_chunk(
    std::size_t maximum_size) {
    if (active_play_file == nullptr || active_play_file->get() == nullptr) {
        return std::nullopt;
    }
    auto bytes = active_play_file->read(maximum_size);
    if (!bytes.has_value()) return std::nullopt;
    const bool end = std::feof(active_play_file->get()) != 0;
    return firmware::application::PlayLineChunk{std::move(*bytes), end};
}

}  // namespace firmware::target
