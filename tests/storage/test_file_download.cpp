// Verifies the host download protocol through a deterministic fake I/O port.
#include "test.hpp"

#include "firmware/application/file_download.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using firmware::application::FileDownload;
using firmware::application::FileDownloadPort;
using firmware::application::HostIdentity;
using firmware::application::HostTransport;
using firmware::core::ByteVector;
using firmware::core::FileCachePaths;
using firmware::core::Frame;

namespace {

ByteVector bytes(std::string_view text) {
    return {text.begin(), text.end()};
}

std::string text(const ByteVector& value) {
    return {value.begin(), value.end()};
}

class FakeDownloadPort final : public FileDownloadPort {
public:
    // Records best-effort cache base preparation.
    void prepare_cache_paths(const FileCachePaths& paths) override {
        prepared_paths = paths;
    }

    // Returns the configured calculated checksum.
    std::optional<std::string> calculate_md5(std::string_view path) override {
        calculated_path = path;
        return calculated_md5;
    }

    // Returns configured sidecar bytes and records its bounded read size.
    std::optional<ByteVector> read_cache(std::string_view path,
                                         std::size_t maximum_size) override {
        cache_path = path;
        cache_maximum = maximum_size;
        return cache_content;
    }

    // Reports whether the configured compressed path exists.
    bool file_exists(std::string_view path) override {
        existence_path = path;
        return compressed_exists;
    }

    // Opens the selected transfer file and returns its configured size.
    std::optional<std::uint64_t> open_file(std::string_view path) override {
        opened_path = path;
        return opened_size;
    }

    // Records one offset read and returns configured content.
    std::optional<ByteVector> read_file(std::uint64_t offset,
                                        std::size_t maximum_size) override {
        read_offset = offset;
        read_maximum = maximum_size;
        ++read_count;
        return read_content;
    }

    // Reports whether the data response workspace is available.
    bool allocate_response_workspace(std::size_t size) override {
        requested_workspace = size;
        return workspace_available;
    }

    // Records closure of the selected file.
    void close_file() override {
        ++close_count;
    }

    // Records a response with its retained destination identity.
    void send(const HostIdentity& host, Frame frame) override {
        destinations.push_back(host);
        sent.push_back(std::move(frame));
    }

    // Records normal or error ownership release.
    void release_ownership() override {
        ++release_count;
    }

    FileCachePaths prepared_paths;
    std::optional<std::string> calculated_md5 =
        std::string("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    std::optional<ByteVector> cache_content;
    std::optional<std::uint64_t> opened_size = 16385U;
    std::optional<ByteVector> read_content = ByteVector({1U, 2U, 3U});
    bool compressed_exists = false;
    bool workspace_available = true;
    std::string calculated_path;
    std::string cache_path;
    std::string existence_path;
    std::string opened_path;
    std::size_t cache_maximum = 0U;
    std::size_t read_maximum = 0U;
    std::size_t requested_workspace = 0U;
    std::size_t read_count = 0U;
    std::size_t close_count = 0U;
    std::size_t release_count = 0U;
    std::uint64_t read_offset = 0U;
    std::vector<HostIdentity> destinations;
    std::vector<Frame> sent;
};

const HostIdentity owner{HostTransport::tcp, 2U, 7U};

}  // namespace

TEST_CASE(hftd_001_config_txt_uses_calculated_md5) {
    FileDownload download;
    FakeDownloadPort port;

    REQUIRE(download.start(owner, "/sd/config.txt", 0U, port));

    REQUIRE_EQ(port.calculated_path, std::string("/sd/config.txt"));
    REQUIRE_EQ(text(port.sent.front().payload),
               std::string("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
}

TEST_CASE(hftd_001_non_config_uses_valid_sidecar_or_exact_fallback) {
    FileDownload download;
    FakeDownloadPort port;
    port.cache_content = bytes("--0123456789ABCDEF0123456789ABCDEF--");

    REQUIRE(download.start(owner, "/sd/job.gcode", 0U, port));
    REQUIRE_EQ(text(port.sent.front().payload),
               std::string("0123456789abcdef0123456789abcdef"));

    FileDownload fallback;
    FakeDownloadPort fallback_port;
    REQUIRE(fallback.start(owner, "/other/job.gcode", 0U, fallback_port));
    REQUIRE_EQ(text(fallback_port.sent.front().payload),
               std::string("82df799dde08f3d86839e24cb97e74d4"));
}

TEST_CASE(hftd_002_existing_compressed_sidecar_is_opened_but_logical_md5_is_announced) {
    FileDownload download;
    FakeDownloadPort port;
    port.compressed_exists = true;

    REQUIRE(download.start(owner, "/sd/gcodes/job.gcode", 0U, port));

    REQUIRE_EQ(port.opened_path, std::string("/sd/gcodes/.lz/job.gcode"));
    REQUIRE_EQ(port.sent.front().type, 0xB1U);
}

TEST_CASE(hftd_003_open_and_md5_failures_send_exact_errors_and_release) {
    FileDownload open_failure;
    FakeDownloadPort open_port;
    open_port.opened_size = std::nullopt;

    REQUIRE(!open_failure.start(owner, "/sd/missing", 0U, open_port));
    REQUIRE_EQ(open_port.sent.back().type, 0xB5U);
    REQUIRE_EQ(text(open_port.sent.back().payload),
               std::string("Error: failed to open file [/missing]!"));
    REQUIRE_EQ(open_port.release_count, 1U);

    FileDownload md5_failure;
    FakeDownloadPort md5_port;
    md5_port.calculated_md5 = std::nullopt;
    REQUIRE(!md5_failure.start(owner, "/sd/config.txt", 0U, md5_port));
    REQUIRE_EQ(text(md5_port.sent.back().payload),
               std::string("Error: failed to get MD5 for [/config.txt]!"));
}

TEST_CASE(hftd_004_successful_open_immediately_sends_md5_to_original_identity) {
    FileDownload download;
    FakeDownloadPort port;

    REQUIRE(download.start(owner, "/sd/job", 0U, port));

    REQUIRE_EQ(port.sent.front().type, 0xB1U);
    REQUIRE_EQ(port.destinations.front(), owner);
}

TEST_CASE(hftd_005_md5_and_geometry_requests_reply_and_geometry_rounds_up) {
    FileDownload download;
    FakeDownloadPort port;
    REQUIRE(download.start(owner, "/sd/job", 0U, port));
    port.sent.clear();

    download.handle({0xB1U, {}}, 1U, port);
    download.handle({0xB2U, {}}, 2U, port);

    REQUIRE_EQ(port.sent[0].type, 0xB1U);
    REQUIRE_EQ(port.sent[1], Frame({0xB2U, {0U, 0U, 0U, 3U, 0x20U, 0U}}));
}

TEST_CASE(hftd_006_data_uses_one_based_sequence_and_8192_byte_blocks) {
    FileDownload download;
    FakeDownloadPort port;
    REQUIRE(download.start(owner, "/sd/job", 0U, port));
    port.sent.clear();

    download.handle({0xB3U, {0U, 0U, 0U, 2U, 0xEEU}}, 1U, port);

    REQUIRE_EQ(port.requested_workspace, 8196U);
    REQUIRE_EQ(port.read_offset, 8192U);
    REQUIRE_EQ(port.read_maximum, 8192U);
    REQUIRE_EQ(port.sent.back(), Frame({0xB3U, {0U, 0U, 0U, 2U, 1U, 2U, 3U}}));
}

TEST_CASE(hftd_007_empty_block_and_workspace_failure_abort_with_exact_errors) {
    FileDownload empty_download;
    FakeDownloadPort empty_port;
    empty_port.read_content = ByteVector{};
    REQUIRE(empty_download.start(owner, "/sd/job", 0U, empty_port));
    empty_download.handle({0xB3U, {0U, 0U, 0U, 1U}}, 1U, empty_port);
    REQUIRE_EQ(text(empty_port.sent.back().payload),
               std::string("Error: Machine received cmd timeout!"));
    REQUIRE(!empty_download.active());

    FileDownload memory_download;
    FakeDownloadPort memory_port;
    memory_port.workspace_available = false;
    REQUIRE(memory_download.start(owner, "/sd/job", 0U, memory_port));
    memory_download.handle({0xB3U, {0U, 0U, 0U, 1U}}, 1U, memory_port);
    REQUIRE_EQ(text(memory_port.sent.back().payload),
               std::string("Error: download_command Memory allocation failed!"));
}

TEST_CASE(hftd_008_retry_rereads_the_last_data_sequence) {
    FileDownload download;
    FakeDownloadPort port;
    REQUIRE(download.start(owner, "/sd/job", 0U, port));
    download.handle({0xB3U, {0U, 0U, 0U, 2U}}, 1U, port);
    port.read_content = ByteVector({9U});

    download.handle({0xB6U, {}}, 2U, port);

    REQUIRE_EQ(port.read_count, 2U);
    REQUIRE_EQ(port.read_offset, 8192U);
    REQUIRE_EQ(port.sent.back().payload, ByteVector({0U, 0U, 0U, 2U, 9U}));
}

TEST_CASE(hftd_009_completion_sends_ack_releases_and_reports_success) {
    FileDownload download;
    FakeDownloadPort port;
    REQUIRE(download.start(owner, "/sd/job", 0U, port));
    port.sent.clear();

    download.handle({0xB4U, {}}, 1U, port);

    REQUIRE_EQ(port.sent.size(), 2U);
    REQUIRE_EQ(port.sent[0], Frame({0xB4U, {'o', 'k', '\r', '\n'}}));
    REQUIRE_EQ(text(port.sent[1].payload), std::string("Info: download success: /job."));
    REQUIRE_EQ(port.close_count, 1U);
    REQUIRE_EQ(port.release_count, 1U);
}

TEST_CASE(hftd_010_remote_cancel_sends_exact_message_and_releases) {
    FileDownload download;
    FakeDownloadPort port;
    REQUIRE(download.start(owner, "/sd/job", 0U, port));

    download.handle({0xB5U, {}}, 1U, port);

    REQUIRE_EQ(text(port.sent.back().payload), std::string("Info: canceled by remote!"));
    REQUIRE(!download.active());
    REQUIRE_EQ(port.release_count, 1U);
}

TEST_CASE(hft_020_and_hft_021_any_frame_restarts_strict_nine_second_timeout) {
    FileDownload download;
    FakeDownloadPort port;
    REQUIRE(download.start(owner, "/sd/job", 100U, port));

    download.handle({0xBFU, {}}, 9000U, port);
    download.poll(18000U, port);
    REQUIRE(download.active());
    download.poll(18001U, port);
    REQUIRE(!download.active());
    REQUIRE_EQ(text(port.sent.back().payload),
               std::string("Error: Machine received cmd timeout!"));
}

TEST_CASE(hft_023_and_hft_025_fifty_one_consecutive_unexpected_packets_abort) {
    FileDownload download;
    FakeDownloadPort port;
    REQUIRE(download.start(owner, "/sd/job", 0U, port));
    for (std::size_t count = 0U; count < 50U; ++count) {
        download.handle({0xBFU, {}}, count + 1U, port);
    }
    REQUIRE(download.active());
    download.handle({0xB1U, {}}, 100U, port);
    for (std::size_t count = 0U; count < 51U; ++count) {
        download.handle({0xBFU, {}}, 101U + count, port);
    }

    REQUIRE(!download.active());
    REQUIRE_EQ(text(port.sent.back().payload),
               std::string("Error: Machine received too many wrong command!"));
}
