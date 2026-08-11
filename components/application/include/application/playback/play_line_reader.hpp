/** @file @brief Defines target-independent logical-line reading for streamed G-code playback. */
#pragma once

#include "core/protocol/bytes.hpp"

#include <cstddef>
#include <optional>

namespace firmware::application {

/// Carries one consumed file chunk and whether it reached physical EOF.
struct PlayLineChunk {
    core::ByteVector bytes;
    bool end_of_file;
};

/// Isolates logical-line rules from the concrete filesystem reader.
class PlayLineSource {
public:
    /// Enables safe destruction through a substituted source implementation.
    virtual ~PlayLineSource() = default;

    /// Reads through LF or the supplied byte limit and reports failure separately.
    virtual std::optional<PlayLineChunk> read_chunk(std::size_t maximum_size) = 0;
};

/** Result of extracting one executable line from streamed-play input. */
enum class PlayLineStatus {
    line,
    end_of_file,
    failure,
};

/// Describes one transformed logical line and its physical EOF state.
struct PlayLineResult {
    PlayLineStatus status;
    core::ByteVector data;
    bool reached_eof;
    /// Bytes counted by goto progress before controller-size normalization.
    std::size_t observed_size = 0U;
};

/// Converts filesystem chunks into the controller's bounded logical-line form.
class PlayLineReader {
public:
    /// Reads one logical line, consuming long-line suffixes through LF or EOF.
    static PlayLineResult read(PlayLineSource& source);
};

}  // namespace firmware::application
