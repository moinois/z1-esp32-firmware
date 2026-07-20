// Verifies streamed-play logical-line reading and wire-size transformation.
#include "test.hpp"

#include "firmware/application/play_line_reader.hpp"

#include <deque>
#include <optional>
#include <string_view>

using firmware::application::PlayLineChunk;
using firmware::application::PlayLineReader;
using firmware::application::PlayLineSource;
using firmware::application::PlayLineStatus;
using firmware::core::ByteVector;

namespace {

ByteVector bytes(std::string_view text) {
    return {text.begin(), text.end()};
}

class FakeLineSource final : public PlayLineSource {
public:
    // Returns queued chunks and records every maximum-size request.
    std::optional<PlayLineChunk> read_chunk(std::size_t maximum_size) override {
        requested_size = maximum_size;
        if (fail || chunks.empty()) {
            return std::nullopt;
        }
        PlayLineChunk chunk = chunks.front();
        chunks.pop_front();
        return chunk;
    }

    bool fail = false;
    std::size_t requested_size = 0U;
    std::deque<PlayLineChunk> chunks;
};

}  // namespace

TEST_CASE(play_014_line_reader_requests_chunks_of_at_most_129_bytes) {
    FakeLineSource source;
    source.chunks.push_back({bytes("G1 X1\n"), false});

    const auto result = PlayLineReader::read(source);

    REQUIRE_EQ(source.requested_size, 129U);
    REQUIRE_EQ(result.status, PlayLineStatus::line);
    REQUIRE_EQ(result.data, bytes("G1 X1\n"));
}

TEST_CASE(play_014_a_line_requiring_multiple_chunks_becomes_one_lf) {
    FakeLineSource source;
    source.chunks.push_back({ByteVector(129U, 'x'), false});
    source.chunks.push_back({bytes("tail\n"), false});

    const auto result = PlayLineReader::read(source);

    REQUIRE_EQ(result.status, PlayLineStatus::line);
    REQUIRE_EQ(result.data, ByteVector({'\n'}));
}

TEST_CASE(play_014_single_chunk_over_64_bytes_replaces_its_64th_byte_with_lf) {
    FakeLineSource source;
    source.chunks.push_back({ByteVector(65U, 'x'), true});

    const auto result = PlayLineReader::read(source);

    REQUIRE_EQ(result.data.size(), 64U);
    REQUIRE_EQ(result.data[62], static_cast<std::uint8_t>('x'));
    REQUIRE_EQ(result.data[63], static_cast<std::uint8_t>('\n'));
    REQUIRE(result.reached_eof);
}

TEST_CASE(play_007_embedded_nul_ends_observed_data_after_consuming_the_chunk) {
    FakeLineSource source;
    source.chunks.push_back({{'G', '1', 0U, 'X', '9', '\n'}, false});

    const auto result = PlayLineReader::read(source);

    REQUIRE_EQ(result.data, bytes("G1"));
    REQUIRE(source.chunks.empty());
}

TEST_CASE(play_007_leading_nul_is_an_empty_non_eof_logical_result) {
    FakeLineSource source;
    source.chunks.push_back({{0U, 'G', '1', '\n'}, false});

    const auto result = PlayLineReader::read(source);

    REQUIRE_EQ(result.status, PlayLineStatus::line);
    REQUIRE(result.data.empty());
    REQUIRE(!result.reached_eof);
}

TEST_CASE(play_014_empty_eof_and_read_failure_are_distinct) {
    FakeLineSource eof_source;
    eof_source.chunks.push_back({{}, true});
    const auto eof = PlayLineReader::read(eof_source);
    REQUIRE_EQ(eof.status, PlayLineStatus::end_of_file);

    FakeLineSource failed_source;
    failed_source.fail = true;
    const auto failed = PlayLineReader::read(failed_source);
    REQUIRE_EQ(failed.status, PlayLineStatus::failure);
}
