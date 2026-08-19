// Verifies streamed-play preparation, identity, error limiting, and status.
#include "test.hpp"

#include "application/playback/play_session.hpp"
#include "core/protocol/crc.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using firmware::application::PlayPreparationPort;
using firmware::application::PlaySession;
using firmware::core::ByteVector;
using firmware::core::Frame;

namespace {

ByteVector bytes(std::string_view text) {
    return {text.begin(), text.end()};
}

class FakePlayPort final : public PlayPreparationPort {
public:
    // Records closure of the previously prepared file.
    void close_file() override {
        ++close_count;
    }

    // Records the resolved path and returns the configured file size.
    std::optional<std::uint64_t> open_file(std::string_view path) override {
        opened_path = path;
        return open_size;
    }

    // Returns the configured cache value for status rendering.
    std::optional<std::string> cached_md5(std::string_view path) override {
        md5_path = path;
        return md5;
    }

    // Records one broadcast console frame.
    void broadcast(Frame frame) override {
        broadcasts.push_back(std::move(frame));
    }

    void diagnose(
        const firmware::application::PlaybackDiagnostic& diagnostic) override {
        diagnostics.push_back(diagnostic);
    }

    std::size_t close_count = 0U;
    std::optional<std::uint64_t> open_size = 123U;
    std::optional<std::string> md5;
    std::string opened_path;
    std::string md5_path;
    std::vector<Frame> broadcasts;
    std::vector<firmware::application::PlaybackDiagnostic> diagnostics;
};

std::string text(const ByteVector& value) {
    return {value.begin(), value.end()};
}

}  // namespace

TEST_CASE(play_001_prepare_closes_old_file_decodes_escapes_and_removes_one_final_lf) {
    PlaySession session;
    FakePlayPort port;
    const ByteVector command{'p', 'l', 'a', 'y', ' ', '/', 's', 'd', '/', 'a', 1U, 'b', '\n'};

    REQUIRE(session.prepare(command, 0U, port));

    REQUIRE_EQ(port.close_count, 0U);
    REQUIRE_EQ(port.opened_path, std::string("/sd/a b"));
    REQUIRE_EQ(port.diagnostics[0].message,
               std::string("收到了play命令准备处理"));
    REQUIRE_EQ(port.diagnostics[1].message,
               std::string("play命令原始文件名: '/sd/a b'"));
}

TEST_CASE(play_002_exact_play_space_treats_the_complete_payload_as_the_path) {
    PlaySession session;
    FakePlayPort port;

    REQUIRE(session.prepare(bytes("play "), 0U, port));

    REQUIRE_EQ(port.opened_path, std::string("/"));
}

TEST_CASE(play_002_nonstandard_play_prefix_is_also_the_complete_path) {
    PlaySession session;
    FakePlayPort port;

    REQUIRE(session.prepare(bytes("playfile.gcode"), 0U, port));

    REQUIRE_EQ(port.opened_path, std::string("/ile.gcode"));
}

TEST_CASE(play_001_and_002_remove_both_supported_play_prefixes) {
    PlaySession session;
    FakePlayPort port;

    REQUIRE(session.prepare(bytes("play play x"), 0U, port));

    REQUIRE_EQ(port.opened_path, std::string("/x"));
    REQUIRE_EQ(port.close_count, 0U);
}

TEST_CASE(play_003_path_identifier_is_common_crc_and_size_is_limited_to_u32) {
    PlaySession session;
    FakePlayPort port;
    REQUIRE(session.prepare(bytes("play /sd/job.gcode"), 0U, port));

    REQUIRE_EQ(session.path_identifier(),
               firmware::core::crc16_ccitt(bytes("/sd/job.gcode")));
    REQUIRE_EQ(session.file_size(), 123U);

    port.open_size = 0x100000000ULL;
    REQUIRE(session.prepare(bytes("play /sd/large.gcode"), 1000U, port));
    REQUIRE_EQ(session.file_size(), 0U);
}

TEST_CASE(play_003_resolved_paths_longer_than_255_bytes_fail) {
    PlaySession session;
    FakePlayPort port;
    const std::string command = "play /" + std::string(255U, 'x');

    REQUIRE(!session.prepare(bytes(command), 0U, port));
    REQUIRE(port.opened_path.empty());
}

TEST_CASE(play_004_open_failure_broadcasts_exact_error_at_most_once_per_second) {
    PlaySession session;
    FakePlayPort port;
    port.open_size = std::nullopt;

    REQUIRE(!session.prepare(bytes("play /one"), 0U, port));
    REQUIRE(!session.prepare(bytes("play /two"), 999U, port));
    REQUIRE(!session.prepare(bytes("play /three"), 1000U, port));

    REQUIRE_EQ(port.broadcasts.size(), 2U);
    REQUIRE_EQ(port.broadcasts.front().type, 0x90U);
    REQUIRE_EQ(text(port.broadcasts.front().payload),
               std::string("Error:open file failed[P0]"));
    REQUIRE_EQ(port.diagnostics[2].message,
               std::string("PLAY_FAIL[P0_OPEN] fopen failed file='/one'"));
}

TEST_CASE(play_005_status_is_empty_until_running_and_uses_only_valid_cached_md5) {
    PlaySession session;
    FakePlayPort port;
    port.md5 = "0123456789abcdef0123456789abcdef";
    REQUIRE(session.prepare(bytes("play /sd/job.gcode"), 0U, port));

    REQUIRE_EQ(text(session.status_reply(port).payload), std::string("|"));
    session.mark_running();
    REQUIRE_EQ(text(session.status_reply(port).payload),
               std::string("/sd/job.gcode|0123456789abcdef0123456789abcdef"));
    port.md5 = "fedcba9876543210fedcba9876543210";
    REQUIRE_EQ(text(session.status_reply(port).payload),
               std::string("/sd/job.gcode|fedcba9876543210fedcba9876543210"));

    port.md5 = "short";
    REQUIRE(session.prepare(bytes("play /sd/next.gcode"), 1U, port));
    REQUIRE_EQ(text(session.status_reply(port).payload), std::string("/sd/next.gcode|"));
}

TEST_CASE(play_006_replacement_prepare_does_not_clear_the_running_flag) {
    PlaySession session;
    FakePlayPort port;
    REQUIRE(session.prepare(bytes("play /first"), 0U, port));
    session.mark_running();

    REQUIRE(session.prepare(bytes("play /second"), 1U, port));

    REQUIRE(session.running());
    REQUIRE_EQ(text(session.status_reply(port).payload), std::string("/second|"));
}

TEST_CASE(play_006_failed_replacement_retains_running_status_and_previous_path) {
    PlaySession session;
    FakePlayPort port;
    REQUIRE(session.prepare(bytes("play /first"), 0U, port));
    session.mark_running();
    port.open_size = std::nullopt;

    REQUIRE(!session.prepare(bytes("play /missing"), 1U, port));

    REQUIRE(session.running());
    REQUIRE_EQ(text(session.status_reply(port).payload), std::string("/first|"));
    REQUIRE_EQ(session.path(), std::string_view("/first"));
    REQUIRE_EQ(session.generation(), 1U);
    REQUIRE_EQ(port.close_count, 0U);
}

TEST_CASE(play_007_embedded_nul_terminates_the_command_path) {
    PlaySession session;
    FakePlayPort port;
    const ByteVector command{'p', 'l', 'a', 'y', ' ', '/', 'o', 'k', 0U, '/', 'i', 'g', 'n'};

    REQUIRE(session.prepare(command, 0U, port));

    REQUIRE_EQ(port.opened_path, std::string("/ok"));
}
