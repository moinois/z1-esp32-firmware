// Verifies filesystem mutations, cache side effects, and exact responses.
#include "test.hpp"

#include "application/storage/filesystem_commands.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

using firmware::application::FilesystemCommandPort;
using firmware::application::FilesystemCommands;
using firmware::core::ByteVector;
using firmware::core::Frame;

namespace {

// Converts command text to the byte representation accepted by services.
ByteVector bytes(std::string_view text) {
    return {text.begin(), text.end()};
}

// Converts a response payload to text for exact assertions.
std::string text(const ByteVector& value) {
    return {value.begin(), value.end()};
}

// Records filesystem mutations and responses without target storage.
class FakeFilesystemCommandPort final : public FilesystemCommandPort {
public:
    // Records directory creation and returns the primary or side-effect result.
    bool create_directory(std::string_view path, std::uint32_t mode) override {
        created_paths.emplace_back(path);
        create_modes.push_back(mode);
        if (created_paths.size() == 1U) {
            return primary_create_succeeds;
        }
        return cache_operations_succeed;
    }

    // Records one best-effort recursive removal.
    void remove_recursively(std::string_view path) override {
        removed_paths.emplace_back(path);
    }

    // Reports whether the requested primary removal root remains.
    bool path_exists(std::string_view path) override {
        existence_queries.emplace_back(path);
        return primary_path_remains;
    }

    // Records rename attempts and returns the primary or side-effect result.
    bool rename_path(std::string_view source,
                     std::string_view destination) override {
        renames.emplace_back(source, destination);
        if (renames.size() == 1U) {
            return primary_rename_succeeds;
        }
        return cache_operations_succeed;
    }

    // Records one response in observable transmission order.
    void send(Frame frame) override {
        sent.push_back(std::move(frame));
    }

    void log_warning(std::string_view message) override {
        warnings.emplace_back(message);
    }

    bool primary_create_succeeds = true;
    bool primary_path_remains = false;
    bool primary_rename_succeeds = true;
    bool cache_operations_succeed = false;
    std::vector<std::string> created_paths;
    std::vector<std::uint32_t> create_modes;
    std::vector<std::string> removed_paths;
    std::vector<std::string> existence_queries;
    std::vector<std::pair<std::string, std::string>> renames;
    std::vector<Frame> sent;
    std::vector<std::string> warnings;
};

}  // namespace

TEST_CASE(file_020_and_021_mkdir_succeeds_then_attempts_both_cache_directories) {
    FakeFilesystemCommandPort port;

    FilesystemCommands::make_directory(bytes(" /sd/gcodes/jobs"), port);

    REQUIRE_EQ(port.created_paths.size(), 3U);
    REQUIRE_EQ(port.created_paths[0], std::string("/sd/gcodes/jobs"));
    REQUIRE_EQ(port.created_paths[1], std::string("/sd/gcodes/.md5/jobs"));
    REQUIRE_EQ(port.created_paths[2], std::string("/sd/gcodes/.lz/jobs"));
    REQUIRE_EQ(port.create_modes[0], 0777U);
    REQUIRE_EQ(port.sent.size(), 2U);
    REQUIRE_EQ(port.sent[0].type, 0x84U);
    REQUIRE_EQ(text(port.sent[0].payload), std::string("ok\r\n"));
    REQUIRE_EQ(port.sent[1].type, 0x90U);
    REQUIRE_EQ(text(port.sent[1].payload),
               std::string("created directory /sd/gcodes/jobs\r\n"));
}

TEST_CASE(file_020_mkdir_failure_sends_only_the_exact_failure) {
    FakeFilesystemCommandPort port;
    port.primary_create_succeeds = false;

    FilesystemCommands::make_directory(bytes(" /sd/jobs"), port);

    REQUIRE_EQ(port.created_paths.size(), 1U);
    REQUIRE_EQ(port.sent.size(), 1U);
    REQUIRE_EQ(port.sent[0].type, 0x85U);
    REQUIRE_EQ(text(port.sent[0].payload),
               std::string("could not create directory /sd/jobs\r\n"));
}

TEST_CASE(file_022_and_023_remove_is_recursive_and_cleans_caches_after_success) {
    FakeFilesystemCommandPort port;

    FilesystemCommands::remove(bytes(" -R /sd/gcodes/jobs"), port);

    REQUIRE_EQ(port.removed_paths.size(), 3U);
    REQUIRE_EQ(port.removed_paths[0], std::string("/sd/gcodes/jobs"));
    REQUIRE_EQ(port.removed_paths[1], std::string("/sd/gcodes/.md5/jobs"));
    REQUIRE_EQ(port.removed_paths[2], std::string("/sd/gcodes/.lz/jobs"));
    REQUIRE_EQ(port.sent.size(), 1U);
    REQUIRE_EQ(port.sent[0].type, 0x84U);
    REQUIRE_EQ(text(port.sent[0].payload), std::string("ok\r\n"));
}

TEST_CASE(file_023_remove_failure_depends_only_on_the_requested_root_remaining) {
    FakeFilesystemCommandPort port;
    port.primary_path_remains = true;

    FilesystemCommands::remove(bytes(" /sd/jobs"), port);

    REQUIRE_EQ(port.removed_paths.size(), 1U);
    REQUIRE_EQ(port.sent.size(), 1U);
    REQUIRE_EQ(port.sent[0].type, 0x85U);
    REQUIRE_EQ(text(port.sent[0].payload),
               std::string("Could not delete /sd/jobs \r\n"));
}

TEST_CASE(file_024_and_025_move_succeeds_then_attempts_equivalent_cache_renames) {
    FakeFilesystemCommandPort port;

    FilesystemCommands::move(bytes(" /sd/gcodes/old /sd/gcodes/new"), port);

    REQUIRE_EQ(port.renames.size(), 3U);
    REQUIRE_EQ(port.renames[0],
               std::make_pair(std::string("/sd/gcodes/old"),
                              std::string("/sd/gcodes/new")));
    REQUIRE_EQ(port.renames[1],
               std::make_pair(std::string("/sd/gcodes/.md5/old"),
                              std::string("/sd/gcodes/.md5/new")));
    REQUIRE_EQ(port.renames[2],
               std::make_pair(std::string("/sd/gcodes/.lz/old"),
                              std::string("/sd/gcodes/.lz/new")));
    REQUIRE_EQ(port.sent.size(), 2U);
    REQUIRE_EQ(port.sent[0].type, 0x84U);
    REQUIRE_EQ(text(port.sent[0].payload), std::string("ok\r\n"));
    REQUIRE_EQ(port.sent[1].type, 0x90U);
    REQUIRE_EQ(text(port.sent[1].payload),
               std::string("renamed /sd/gcodes/old to /sd/gcodes/new\r\n"));
}

TEST_CASE(file_025_move_failure_sends_only_the_exact_failure) {
    FakeFilesystemCommandPort port;
    port.primary_rename_succeeds = false;

    FilesystemCommands::move(bytes(" old new"), port);

    REQUIRE_EQ(port.renames.size(), 1U);
    REQUIRE_EQ(port.sent.size(), 1U);
    REQUIRE_EQ(port.sent[0].type, 0x85U);
    REQUIRE_EQ(text(port.sent[0].payload),
               std::string("Could not rename /old to /new\r\n"));
}

TEST_CASE(diag_028_move_without_separator_logs_the_exact_warning) {
    FakeFilesystemCommandPort port;

    FilesystemCommands::move(bytes(" only-one-path"), port);

    REQUIRE_EQ(port.warnings,
               std::vector<std::string>({"mv: missing separator in params"}));
    REQUIRE(port.renames.empty());
    REQUIRE(port.sent.empty());
}

TEST_CASE(diag_028_move_with_an_empty_path_logs_the_exact_warning) {
    FakeFilesystemCommandPort port;

    FilesystemCommands::move(bytes(" source "), port);

    REQUIRE_EQ(port.warnings,
               std::vector<std::string>({"mv: empty from/to path"}));
    REQUIRE(port.renames.empty());
    REQUIRE(port.sent.empty());
}

TEST_CASE(file_026_ftype_always_reports_nc) {
    FakeFilesystemCommandPort port;

    FilesystemCommands::file_type(port);

    REQUIRE_EQ(port.sent.size(), 1U);
    REQUIRE_EQ(port.sent[0].type, 0x90U);
    REQUIRE_EQ(text(port.sent[0].payload), std::string("ftype = nc\r\n"));
}

TEST_CASE(file_020_to_025_invalid_arguments_are_silent) {
    FakeFilesystemCommandPort port;

    FilesystemCommands::make_directory(bytes("   "), port);
    FilesystemCommands::remove(bytes(" -R   "), port);
    FilesystemCommands::move(bytes(" only-one-path"), port);

    REQUIRE(port.created_paths.empty());
    REQUIRE(port.removed_paths.empty());
    REQUIRE(port.renames.empty());
    REQUIRE(port.sent.empty());
}

TEST_CASE(file_021_and_023_ordinary_paths_apply_only_available_cache_mapping) {
    FakeFilesystemCommandPort create_port;
    FilesystemCommands::make_directory(bytes(" /sd/jobs"), create_port);
    REQUIRE_EQ(create_port.created_paths,
               std::vector<std::string>({"/sd/jobs", "/sd/.md5/jobs"}));

    FakeFilesystemCommandPort remove_port;
    FilesystemCommands::remove(bytes(" /sd/jobs"), remove_port);
    REQUIRE_EQ(remove_port.removed_paths,
               std::vector<std::string>({"/sd/jobs", "/sd/.md5/jobs"}));

    FakeFilesystemCommandPort move_port;
    FilesystemCommands::move(bytes(" /sd/old /sd/new"), move_port);
    REQUIRE_EQ(move_port.renames.size(), 2U);
    REQUIRE_EQ(move_port.renames[1],
               std::make_pair(std::string("/sd/.md5/old"),
                              std::string("/sd/.md5/new")));
}
