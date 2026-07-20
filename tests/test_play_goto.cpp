// Verifies streamed-play goto validation, scanning, progress, and failures.
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

class FakeGotoPort final : public PlayControllerPort {
public:
    // Records file closure.
    void close_file() override {
        ++close_count;
    }

    // Opens a fixed-size fake file.
    std::optional<std::uint64_t> open_file(std::string_view) override {
        return 100U;
    }

    // Returns no checksum cache value.
    std::optional<std::string> cached_md5(std::string_view) override {
        return std::nullopt;
    }

    // Records one console broadcast.
    void broadcast(Frame frame) override {
        broadcasts.push_back(std::move(frame));
    }

    // Records one controller response.
    bool send(Frame frame) override {
        sent.push_back(std::move(frame));
        return true;
    }

    // Ignores observer state for goto tests.
    void play_state_changed(bool) override {}

    // Records ownership release, which goto must not perform.
    void release_play_ownership() override {
        ++release_count;
    }

    // Rewinds the fake line sequence.
    bool rewind_file() override {
        ++rewind_count;
        chunks = initial_chunks;
        return rewind_succeeds;
    }

    // Reads one chunk and advances the fake monotonic clock.
    std::optional<PlayLineChunk> read_chunk(std::size_t) override {
        if (read_fails || chunks.empty()) {
            return std::nullopt;
        }
        PlayLineChunk chunk = chunks.front();
        chunks.pop_front();
        clock += milliseconds_per_read;
        return chunk;
    }

    // Returns the fake monotonic clock used by progress pacing.
    std::uint64_t now_milliseconds() const override {
        return clock;
    }

    void set_lines(std::initializer_list<PlayLineChunk> values) {
        initial_chunks = values;
        chunks = initial_chunks;
    }

    bool rewind_succeeds = true;
    bool read_fails = false;
    std::uint64_t clock = 0U;
    std::uint64_t milliseconds_per_read = 0U;
    std::size_t close_count = 0U;
    std::size_t release_count = 0U;
    std::size_t rewind_count = 0U;
    std::deque<PlayLineChunk> initial_chunks;
    std::deque<PlayLineChunk> chunks;
    std::vector<Frame> sent;
    std::vector<Frame> broadcasts;
};

ByteVector goto_request(std::uint16_t identifier, std::uint32_t line) {
    return {
        static_cast<std::uint8_t>(identifier >> 8U),
        static_cast<std::uint8_t>(identifier),
        static_cast<std::uint8_t>(line >> 24U),
        static_cast<std::uint8_t>(line >> 16U),
        static_cast<std::uint8_t>(line >> 8U),
        static_cast<std::uint8_t>(line),
    };
}

}  // namespace

TEST_CASE(play_019_invalid_goto_sends_f4_and_exact_check_error) {
    PlaySession session;
    PlayController controller(session);
    FakeGotoPort port;

    controller.handle({0xF6U, {}}, 0U, port);

    REQUIRE_EQ(port.sent.back().type, 0xF4U);
    REQUIRE_EQ(port.broadcasts.back().payload,
               bytes("Error:goto check failed.file does not exist or CRC wrong [P3]"));
}

TEST_CASE(play_019_short_valid_identifier_sends_f4_and_exact_format_error) {
    PlaySession session;
    PlayController controller(session);
    FakeGotoPort port;
    session.prepare(bytes("play /job"), 0U, port);
    const std::uint16_t identifier = session.path_identifier();

    controller.handle({0xF6U, {static_cast<std::uint8_t>(identifier >> 8U),
                               static_cast<std::uint8_t>(identifier)}},
                      0U, port);

    REQUIRE_EQ(port.sent.back().type, 0xF4U);
    REQUIRE_EQ(port.broadcasts.back().payload,
               bytes("Error:PTYPE_PLAY_DATA goto cmd format error [P3]"));
}

TEST_CASE(play_020_rewind_failure_sends_f5_without_cleanup) {
    PlaySession session;
    PlayController controller(session);
    FakeGotoPort port;
    session.prepare(bytes("play /job"), 0U, port);
    port.rewind_succeeds = false;

    controller.handle({0xF6U, goto_request(session.path_identifier(), 1U)}, 0U, port);

    REQUIRE_EQ(port.sent.back().type, 0xF5U);
    REQUIRE(session.file_open());
    REQUIRE_EQ(port.release_count, 0U);
}

TEST_CASE(play_020_target_zero_reports_after_the_first_nonempty_line) {
    PlaySession session;
    PlayController controller(session);
    FakeGotoPort port;
    session.prepare(bytes("play /job"), 0U, port);
    const std::uint16_t identifier = session.path_identifier();
    port.set_lines({{{0U, 'x', '\n'}, false}, {bytes("G1\n"), false}});

    controller.handle({0xF6U, goto_request(identifier, 0U)}, 0U, port);

    REQUIRE_EQ(port.sent.size(), 1U);
    REQUIRE_EQ(port.sent.back(),
               Frame({0xF7U, {static_cast<std::uint8_t>(identifier >> 8U),
                               static_cast<std::uint8_t>(identifier), 0U, 0U, 0U, 2U,
                               0U, 0U, 0U, 3U}}));
}

TEST_CASE(play_020_progress_is_sent_only_after_more_than_100_ms) {
    PlaySession session;
    PlayController controller(session);
    FakeGotoPort port;
    session.prepare(bytes("play /job"), 0U, port);
    const std::uint16_t identifier = session.path_identifier();
    port.milliseconds_per_read = 51U;
    port.set_lines({{bytes("a\n"), false}, {bytes("b\n"), false},
                    {bytes("c\n"), false}, {bytes("d\n"), false}});

    controller.handle({0xF6U, goto_request(identifier, 4U)}, 0U, port);

    REQUIRE_EQ(port.sent.size(), 2U);
    REQUIRE_EQ(port.sent[0].type, 0xF7U);
    REQUIRE_EQ(port.sent[0].payload[5], 2U);
    REQUIRE_EQ(port.sent[1].payload[5], 4U);
}

TEST_CASE(play_020_target_at_eof_sends_two_identical_final_progress_packets) {
    PlaySession session;
    PlayController controller(session);
    FakeGotoPort port;
    session.prepare(bytes("play /job"), 0U, port);
    const std::uint16_t identifier = session.path_identifier();
    port.set_lines({{bytes("last\n"), true}});

    controller.handle({0xF6U, goto_request(identifier, 0U)}, 0U, port);

    REQUIRE_EQ(port.sent.size(), 2U);
    REQUIRE_EQ(port.sent[0].type, 0xF7U);
    REQUIRE_EQ(port.sent[1], port.sent[0]);
    REQUIRE(session.file_open());
}
