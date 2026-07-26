// Verifies streamed-play controller start and terminal lifecycle behavior.
#include "test.hpp"

#include "firmware/application/play_controller.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using firmware::application::PlayController;
using firmware::application::PlayControllerPort;
using firmware::application::PlaySession;
using firmware::core::ByteVector;
using firmware::core::Frame;

namespace {

ByteVector bytes(std::string_view text) {
    return {text.begin(), text.end()};
}

class FakePlayControllerPort final : public PlayControllerPort {
public:
    // Records closure of a prepared file.
    void close_file() override {
        ++close_count;
    }

    // Opens the configured fake file.
    std::optional<std::uint64_t> open_file(std::string_view path) override {
        opened_path = path;
        return open_size;
    }

    // Returns no MD5 cache value for lifecycle tests.
    std::optional<std::string> cached_md5(std::string_view) override {
        return std::nullopt;
    }

    // Records one host broadcast.
    void broadcast(Frame frame) override {
        broadcasts.push_back(std::move(frame));
    }

    // Records one controller response and reports queue acceptance.
    bool send(Frame frame) override {
        sent.push_back(std::move(frame));
        return send_succeeds;
    }

    // Records a running or stopped observer notification.
    void play_state_changed(bool running) override {
        state_changes.push_back(running);
    }

    // Records release of the physical play owner.
    void release_play_ownership() override {
        ++release_count;
    }

    // Reports successful rewind for lifecycle-only tests.
    bool rewind_file() override {
        return true;
    }

    // Reports no readable data for lifecycle-only tests.
    std::optional<firmware::application::PlayLineChunk> read_chunk(std::size_t) override {
        return std::nullopt;
    }

    // Returns a fixed clock for lifecycle-only tests.
    std::uint64_t now_milliseconds() const override {
        return 0U;
    }

    bool send_succeeds = true;
    std::optional<std::uint64_t> open_size = 0x01020304U;
    std::string opened_path;
    std::size_t close_count = 0U;
    std::size_t release_count = 0U;
    std::vector<Frame> sent;
    std::vector<Frame> broadcasts;
    std::vector<bool> state_changes;
};

}  // namespace

TEST_CASE(play_010_invalid_start_sends_f5_and_exact_rate_limited_error_without_cleanup) {
    PlaySession session;
    PlayController controller(session);
    FakePlayControllerPort port;
    REQUIRE(session.prepare(bytes("play /job"), 0U, port));
    const std::size_t closes_before = port.close_count;

    controller.handle({0xF1U, {0U}}, 0U, port);
    controller.handle({0xF1U, {0U}}, 500U, port);

    REQUIRE_EQ(port.sent.size(), 2U);
    REQUIRE_EQ(port.sent.front(), Frame({0xF5U, {}}));
    REQUIRE_EQ(port.broadcasts.size(), 1U);
    REQUIRE_EQ(port.broadcasts.front().payload,
               bytes("Error:start check failed.file does not exist or CRC wrong [P1]"));
    REQUIRE_EQ(port.close_count, closes_before);
    REQUIRE(session.file_open());
}

TEST_CASE(play_010_mismatched_identifier_keeps_file_available_for_retry) {
    PlaySession session;
    PlayController controller(session);
    FakePlayControllerPort port;
    REQUIRE(session.prepare(bytes("play /job"), 0U, port));

    controller.handle({0xF1U, {0U, 0U}}, 0U, port);

    REQUIRE_EQ(port.sent.back().type, 0xF5U);
    REQUIRE(session.file_open());
    REQUIRE_EQ(port.release_count, 0U);
}

TEST_CASE(play_011_valid_start_returns_identifier_and_big_endian_file_size) {
    PlaySession session;
    PlayController controller(session);
    FakePlayControllerPort port;
    REQUIRE(session.prepare(bytes("play /job"), 0U, port));
    const std::uint16_t identifier = session.path_identifier();

    controller.handle({0xF1U,
                       {static_cast<std::uint8_t>(identifier >> 8U),
                        static_cast<std::uint8_t>(identifier)}},
                      1U, port);

    REQUIRE_EQ(port.sent.back(),
               Frame({0xF2U,
                      {static_cast<std::uint8_t>(identifier >> 8U),
                       static_cast<std::uint8_t>(identifier), 1U, 2U, 3U, 4U}}));
    REQUIRE(session.running());
    REQUIRE_EQ(port.state_changes.back(), true);
}

TEST_CASE(play_011_repeated_valid_start_repeats_reply_and_notification) {
    PlaySession session;
    PlayController controller(session);
    FakePlayControllerPort port;
    REQUIRE(session.prepare(bytes("play /job"), 0U, port));
    const std::uint16_t identifier = session.path_identifier();
    const Frame start{0xF1U, {static_cast<std::uint8_t>(identifier >> 8U),
                              static_cast<std::uint8_t>(identifier)}};

    controller.handle(start, 1U, port);
    controller.handle(start, 2U, port);

    REQUIRE_EQ(port.sent.size(), 2U);
    REQUIRE_EQ(port.state_changes.size(), 2U);
}

TEST_CASE(play_018_f4_and_f5_cleanup_without_protocol_reply) {
    for (const std::uint8_t terminal_type : {0xF4U, 0xF5U}) {
        PlaySession session;
        PlayController controller(session);
        FakePlayControllerPort port;
        REQUIRE(session.prepare(bytes("play /job"), 0U, port));
        session.mark_running();
        const std::size_t sends_before = port.sent.size();

        controller.handle({terminal_type, {}}, 1U, port);

        REQUIRE_EQ(port.sent.size(), sends_before);
        REQUIRE(!session.running());
        REQUIRE(!session.file_open());
        REQUIRE_EQ(port.state_changes.back(), false);
        REQUIRE_EQ(port.release_count, 1U);
    }
}

TEST_CASE(play_018_incoming_f2_and_f7_are_ignored) {
    PlaySession session;
    PlayController controller(session);
    FakePlayControllerPort port;

    controller.handle({0xF2U, {}}, 0U, port);
    controller.handle({0xF7U, {}}, 0U, port);

    REQUIRE(port.sent.empty());
    REQUIRE(port.broadcasts.empty());
    REQUIRE(port.state_changes.empty());
}
