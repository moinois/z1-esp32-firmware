// Verifies MD5 command validation, hashing policy, and exact response text.
#include "test.hpp"

#include "firmware/application/file_hash_command.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using firmware::application::FileHashCommand;
using firmware::application::FileHashPathState;
using firmware::application::FileHashPort;
using firmware::core::ByteVector;
using firmware::core::Frame;

namespace {

// Converts command text to the byte representation accepted by the service.
ByteVector bytes(std::string_view text) {
    return {text.begin(), text.end()};
}

// Converts a response payload to text for exact assertions.
std::string text(const ByteVector& value) {
    return {value.begin(), value.end()};
}

// Supplies deterministic metadata and hash results without a real filesystem.
class FakeFileHashPort final : public FileHashPort {
public:
    // Returns the configured resolution and path-kind result.
    FileHashPathState inspect_path(std::string_view path) override {
        inspected_path = path;
        return path_state;
    }

    // Returns the configured hash and records the required read-block size.
    std::optional<std::string> calculate_md5(std::string_view path,
                                             std::size_t block_size) override {
        hashed_path = path;
        hash_block_size = block_size;
        return hash;
    }

    // Records one response frame.
    void send(Frame frame) override {
        sent.push_back(std::move(frame));
    }

    FileHashPathState path_state = FileHashPathState::regular_file;
    std::optional<std::string> hash =
        std::string("0123456789abcdef0123456789abcdef");
    std::string inspected_path;
    std::string hashed_path;
    std::size_t hash_block_size = 0U;
    std::vector<Frame> sent;
};

}  // namespace

TEST_CASE(file_027_empty_md5_argument_sends_the_required_usage_error) {
    FakeFileHashPort port;

    FileHashCommand::execute(bytes("  \t\r\n"), port);

    REQUIRE_EQ(port.sent.size(), 1U);
    REQUIRE_EQ(port.sent[0].type, 0x90U);
    REQUIRE_EQ(text(port.sent[0].payload),
               std::string("Error: md5sum requires a file path\r\n"));
}

TEST_CASE(file_027_resolution_failure_is_silent) {
    FakeFileHashPort port;
    port.path_state = FileHashPathState::resolution_failure;

    FileHashCommand::execute(bytes(" file.bin"), port);

    REQUIRE(port.sent.empty());
}

TEST_CASE(file_028_missing_nonregular_and_hash_failures_have_exact_messages) {
    FakeFileHashPort missing;
    missing.path_state = FileHashPathState::missing;
    FileHashCommand::execute(bytes(" file.bin"), missing);
    REQUIRE_EQ(text(missing.sent[0].payload),
               std::string("Error: file not found [/file.bin]\r\n"));

    FakeFileHashPort nonregular;
    nonregular.path_state = FileHashPathState::not_regular;
    FileHashCommand::execute(bytes(" folder"), nonregular);
    REQUIRE_EQ(text(nonregular.sent[0].payload),
               std::string("Error: not a file [/folder]\r\n"));

    FakeFileHashPort failed;
    failed.hash = std::nullopt;
    FileHashCommand::execute(bytes(" file.bin"), failed);
    REQUIRE_EQ(text(failed.sent[0].payload),
               std::string("Error: md5sum failed [/file.bin]\r\n"));
}

TEST_CASE(file_029_success_hashes_in_4096_byte_blocks_and_emits_lowercase_without_separator) {
    FakeFileHashPort port;
    port.hash = std::string("ABCDEF0123456789ABCDEF0123456789");

    FileHashCommand::execute(bytes(" /sd/my\x01" "file.bin"), port);

    REQUIRE_EQ(port.inspected_path, std::string("/sd/my file.bin"));
    REQUIRE_EQ(port.hashed_path, std::string("/sd/my file.bin"));
    REQUIRE_EQ(port.hash_block_size, 4096U);
    REQUIRE_EQ(port.sent.size(), 1U);
    REQUIRE_EQ(port.sent[0].type, 0x90U);
    REQUIRE_EQ(text(port.sent[0].payload),
               std::string("abcdef0123456789abcdef0123456789/sd/my file.bin\r\n"));
}
