// Verifies streamed-play data validation, aggregation, rewind, retry, and EOF.
#include "test.hpp"

#include "firmware/application/play_controller.hpp"

#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using firmware::application::PlayController;
using firmware::application::PlayControllerPort;
using firmware::application::PlayLineChunk;
using firmware::application::PlaySession;
using firmware::core::ByteVector;
using firmware::core::Frame;

namespace {

ByteVector bytes(std::string_view text) {
    return {text.begin(), text.end()};
}

class FakePlayDataPort final : public PlayControllerPort {
public:
    // Records closure of the prepared file.
    void close_file() override {
        ++close_count;
    }

    // Opens the configured in-memory file.
    std::optional<std::uint64_t> open_file(std::string_view) override {
        return 100U;
    }

    // Returns no cache checksum for data tests.
    std::optional<std::string> cached_md5(std::string_view) override {
        return std::nullopt;
    }

    // Records one host broadcast.
    void broadcast(Frame frame) override {
        broadcasts.push_back(std::move(frame));
    }

    // Records one controller response.
    bool send(Frame frame) override {
        sent.push_back(std::move(frame));
        return true;
    }

    // Records observer state changes.
    void play_state_changed(bool running) override {
        states.push_back(running);
    }

    // Records ownership release.
    void release_play_ownership() override {
        ++release_count;
    }

    // Rewinds the queued source to its configured initial chunks.
    bool rewind_file() override {
        ++rewind_count;
        chunks = initial_chunks;
        return rewind_succeeds;
    }

    // Returns one queued line-reader chunk.
    std::optional<PlayLineChunk> read_chunk(std::size_t maximum_size) override {
        requested_chunk_size = maximum_size;
        if (read_fails || chunks.empty()) {
            return std::nullopt;
        }
        PlayLineChunk chunk = chunks.front();
        chunks.pop_front();
        return chunk;
    }

    // Returns a fixed clock for data-only tests.
    std::uint64_t now_milliseconds() const override {
        return 0U;
    }

    void set_lines(std::initializer_list<PlayLineChunk> values) {
        initial_chunks = values;
        chunks = initial_chunks;
    }

    bool rewind_succeeds = true;
    bool read_fails = false;
    std::size_t close_count = 0U;
    std::size_t release_count = 0U;
    std::size_t rewind_count = 0U;
    std::size_t requested_chunk_size = 0U;
    std::deque<PlayLineChunk> initial_chunks;
    std::deque<PlayLineChunk> chunks;
    std::vector<Frame> sent;
    std::vector<Frame> broadcasts;
    std::vector<bool> states;
};

ByteVector data_request(std::uint16_t identifier, std::uint32_t index,
                        std::optional<std::uint16_t> maximum_lines = std::nullopt) {
    ByteVector payload{
        static_cast<std::uint8_t>(identifier >> 8U),
        static_cast<std::uint8_t>(identifier),
        static_cast<std::uint8_t>(index >> 24U),
        static_cast<std::uint8_t>(index >> 16U),
        static_cast<std::uint8_t>(index >> 8U),
        static_cast<std::uint8_t>(index),
    };
    if (maximum_lines.has_value()) {
        payload.push_back(static_cast<std::uint8_t>(*maximum_lines >> 8U));
        payload.push_back(static_cast<std::uint8_t>(*maximum_lines));
    }
    return payload;
}

void prepare_running(PlaySession& session, PlayController& controller,
                     FakePlayDataPort& port) {
    session.prepare(bytes("play /job"), 0U, port);
    const std::uint16_t identifier = session.path_identifier();
    controller.handle({0xF1U, {static_cast<std::uint8_t>(identifier >> 8U),
                               static_cast<std::uint8_t>(identifier)}},
                      0U, port);
    port.sent.clear();
}

}  // namespace

TEST_CASE(play_013_no_file_or_mismatched_identifier_sends_f4_without_format_error) {
    PlaySession session;
    PlayController controller(session);
    FakePlayDataPort port;

    controller.handle({0xF3U, {}}, 0U, port);
    REQUIRE_EQ(port.sent.back(), Frame({0xF4U, {}}));
    REQUIRE(port.broadcasts.empty());

    session.prepare(bytes("play /job"), 1U, port);
    controller.handle({0xF3U, {0U, 0U}}, 1U, port);
    REQUIRE(port.broadcasts.empty());
}

TEST_CASE(play_013_short_data_request_with_open_file_sends_f4_and_exact_error) {
    PlaySession session;
    PlayController controller(session);
    FakePlayDataPort port;
    session.prepare(bytes("play /job"), 0U, port);
    const std::uint16_t identifier = session.path_identifier();

    controller.handle({0xF3U, {static_cast<std::uint8_t>(identifier >> 8U),
                               static_cast<std::uint8_t>(identifier)}},
                      0U, port);

    REQUIRE_EQ(port.sent.back().type, 0xF4U);
    REQUIRE_EQ(port.broadcasts.back().payload,
               bytes("Error:PTYPE_PLAY_DATA command data format error [P2]"));
}

TEST_CASE(play_012_missing_or_zero_line_limit_means_255) {
    PlaySession session;
    PlayController controller(session);
    FakePlayDataPort port;
    port.set_lines({{bytes("one\n"), false}, {bytes("two\n"), true}});
    prepare_running(session, controller, port);
    const std::uint16_t identifier = session.path_identifier();

    controller.handle({0xF3U, data_request(identifier, 0U, 0U)}, 1U, port);

    REQUIRE_EQ(port.sent.front().payload,
               ByteVector({static_cast<std::uint8_t>(identifier >> 8U),
                           static_cast<std::uint8_t>(identifier), 0U, 0U, 0U, 0U,
                           'o', 'n', 'e', '\n', 't', 'w', 'o', '\n'}));
}

TEST_CASE(play_015_data_respects_requested_line_count_and_repeats_first_six_bytes) {
    PlaySession session;
    PlayController controller(session);
    FakePlayDataPort port;
    port.set_lines({{bytes("one\n"), false}, {bytes("two\n"), false}});
    prepare_running(session, controller, port);
    const ByteVector request = data_request(session.path_identifier(), 0U, 1U);

    controller.handle({0xF3U, request}, 1U, port);

    ByteVector expected(request.begin(), request.begin() + 6);
    expected.insert(expected.end(), {'o', 'n', 'e', '\n'});
    REQUIRE_EQ(port.sent.back(), Frame({0xF3U, expected}));
}

TEST_CASE(play_015_aggregation_stops_when_fewer_than_74_data_bytes_remain) {
    PlaySession session;
    PlayController controller(session);
    FakePlayDataPort port;
    std::deque<PlayLineChunk> lines;
    for (std::size_t index = 0U; index < 7U; ++index) {
        ByteVector line(64U, 'x');
        line.back() = '\n';
        lines.push_back({std::move(line), false});
    }
    port.initial_chunks = lines;
    port.chunks = lines;
    prepare_running(session, controller, port);

    controller.handle({0xF3U, data_request(session.path_identifier(), 0U)}, 1U, port);

    REQUIRE_EQ(port.sent.back().payload.size(), 454U);
}

TEST_CASE(play_016_same_index_resends_retained_nonempty_result_without_reading) {
    PlaySession session;
    PlayController controller(session);
    FakePlayDataPort port;
    port.set_lines({{bytes("one\n"), false}, {bytes("two\n"), false}});
    prepare_running(session, controller, port);
    const ByteVector request = data_request(session.path_identifier(), 0U, 1U);
    controller.handle({0xF3U, request}, 1U, port);
    const std::size_t remaining = port.chunks.size();
    const Frame first_reply = port.sent.back();

    controller.handle({0xF3U, request}, 2U, port);

    REQUIRE_EQ(port.sent.back(), first_reply);
    REQUIRE_EQ(port.chunks.size(), remaining);
}

TEST_CASE(play_016_out_of_position_index_rewinds_and_skips_from_start) {
    PlaySession session;
    PlayController controller(session);
    FakePlayDataPort port;
    port.set_lines({{bytes("zero\n"), false}, {bytes("one\n"), false},
                    {bytes("two\n"), false}});
    prepare_running(session, controller, port);

    controller.handle({0xF3U, data_request(session.path_identifier(), 2U, 1U)}, 1U, port);

    REQUIRE_EQ(port.rewind_count, 1U);
    REQUIRE_EQ(port.sent.back().payload.back(), static_cast<std::uint8_t>('\n'));
    REQUIRE_EQ(port.sent.back().payload[6], static_cast<std::uint8_t>('t'));
}

TEST_CASE(play_016_rewind_failure_sends_f4_without_closing_or_releasing) {
    PlaySession session;
    PlayController controller(session);
    FakePlayDataPort port;
    port.set_lines({{bytes("zero\n"), false}});
    prepare_running(session, controller, port);
    port.rewind_succeeds = false;

    controller.handle({0xF3U, data_request(session.path_identifier(), 2U)}, 1U, port);

    REQUIRE_EQ(port.sent.back().type, 0xF4U);
    REQUIRE(session.file_open());
    REQUIRE_EQ(port.release_count, 0U);
}

TEST_CASE(play_017_eof_sends_final_data_then_f4_and_performs_cleanup) {
    PlaySession session;
    PlayController controller(session);
    FakePlayDataPort port;
    port.set_lines({{bytes("last\n"), true}});
    prepare_running(session, controller, port);

    controller.handle({0xF3U, data_request(session.path_identifier(), 0U)}, 1U, port);

    REQUIRE_EQ(port.sent.size(), 2U);
    REQUIRE_EQ(port.sent[0].type, 0xF3U);
    REQUIRE_EQ(port.sent[1].type, 0xF4U);
    REQUIRE(!session.file_open());
    REQUIRE_EQ(port.states.back(), false);
    REQUIRE_EQ(port.release_count, 1U);
}

TEST_CASE(play_007_empty_nul_line_produces_no_reply_and_leaves_play_open) {
    PlaySession session;
    PlayController controller(session);
    FakePlayDataPort port;
    port.set_lines({{{0U, 'x', '\n'}, false}});
    prepare_running(session, controller, port);

    controller.handle({0xF3U, data_request(session.path_identifier(), 0U)}, 1U, port);

    REQUIRE(port.sent.empty());
    REQUIRE(session.file_open());
}
