// Verifies a sanitized desktop-client initialization trace against production policies.
#include "test.hpp"

#include "application/controller/controller_snapshots.hpp"
#include "application/storage/directory_listing.hpp"
#include "application/storage/file_download.hpp"
#include "application/storage/filesystem_commands.hpp"
#include "core/protocol/frame.hpp"
#include "core/filesystem/file_transfer_paths.hpp"
#include "core/protocol/protocol_constants.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using firmware::application::ControllerSnapshots;
using firmware::application::DirectoryEntry;
using firmware::application::DirectoryListPort;
using firmware::application::FileDownload;
using firmware::application::FileDownloadPort;
using firmware::application::FilesystemCommandPort;
using firmware::application::FilesystemCommands;
using firmware::application::HostIdentity;
using firmware::application::HostTransport;
using firmware::core::ByteVector;
using firmware::core::FileCachePaths;
using firmware::core::Frame;

namespace {

ByteVector bytes(std::string_view value) {
    return {value.begin(), value.end()};
}

std::string text(const ByteVector& value) {
    return {value.begin(), value.end()};
}

// Provides deterministic config bytes behind the production download state machine.
class TraceDownloadPort final : public FileDownloadPort {
public:
    void prepare_cache_paths(const FileCachePaths&) override {}

    std::optional<std::string> calculate_md5(std::string_view) override {
        return storage_available
            ? std::optional<std::string>("0123456789abcdef0123456789abcdef")
            : std::nullopt;
    }

    std::optional<ByteVector> read_cache(std::string_view, std::size_t) override {
        return std::nullopt;
    }

    bool file_exists(std::string_view) override {
        return false;
    }

    std::optional<std::uint64_t> open_file(std::string_view) override {
        return storage_available
            ? std::optional<std::uint64_t>(config.size())
            : std::nullopt;
    }

    std::optional<ByteVector> read_file(std::uint64_t offset,
                                        std::size_t maximum_size) override {
        if (offset >= config.size()) {
            return std::nullopt;
        }
        const std::size_t start = static_cast<std::size_t>(offset);
        const std::size_t count = std::min(maximum_size, config.size() - start);
        return ByteVector(config.begin() + static_cast<std::ptrdiff_t>(start),
                          config.begin() + static_cast<std::ptrdiff_t>(start + count));
    }

    bool allocate_response_workspace(std::size_t) override {
        return true;
    }

    void close_file() override {}

    void send(const HostIdentity&, Frame frame) override {
        sent.push_back(std::move(frame));
    }

    void release_ownership() override {
        ++release_count;
    }

    bool storage_available = true;
    ByteVector config = bytes("wifi.machine_name = simulated\n");
    std::vector<Frame> sent;
    std::size_t release_count = 0U;
};

// Returns either an empty directory or a real open failure to the listing policy.
class TraceDirectoryPort final : public DirectoryListPort {
public:
    std::optional<std::vector<DirectoryEntry>> list_directory(
        std::string_view) override {
        return storage_available
            ? std::optional<std::vector<DirectoryEntry>>(std::vector<DirectoryEntry>{})
            : std::nullopt;
    }

    void send(Frame frame) override {
        sent.push_back(std::move(frame));
    }

    bool storage_available = true;
    std::vector<Frame> sent;
};

// Captures the fixed local ftype response without exercising mutations.
class TraceFilesystemPort final : public FilesystemCommandPort {
public:
    bool create_directory(std::string_view, std::uint32_t) override { return false; }
    void remove_recursively(std::string_view) override {}
    bool path_exists(std::string_view) override { return false; }
    bool rename_path(std::string_view, std::string_view) override { return false; }
    void send(Frame frame) override { sent.push_back(std::move(frame)); }

    std::vector<Frame> sent;
};

// Round-trips a trace through arbitrary 64-byte transport packet boundaries.
void require_fragmented_round_trip(const std::vector<Frame>& trace) {
    firmware::core::StreamDecoder decoder(firmware::core::StreamPolicy::usb());
    std::vector<Frame> decoded;
    for (const Frame& frame : trace) {
        const ByteVector encoded = firmware::core::encode_frame(frame);
        for (std::size_t offset = 0U; offset < encoded.size(); offset += 64U) {
            const std::size_t count = std::min<std::size_t>(64U, encoded.size() - offset);
            const auto received = decoder.push(
                {encoded.data() + offset, count});
            decoded.insert(decoded.end(), received.begin(), received.end());
        }
    }
    REQUIRE_EQ(decoded, trace);
}

}  // namespace

TEST_CASE(compat_studio_trace_preserves_observed_request_framing) {
    const std::vector<Frame> requests{
        {0xA1U, bytes("?")},
        {0xB0U, bytes("download /sd/config.txt")},
        {0xA2U, bytes("time")},
        {0xA2U, bytes("model")},
        {0xA2U, bytes("version")},
        {0xA2U, bytes("ftype")},
        {0xA2U, bytes("ls -e -s /sd/gcodes")},
        {0xB7U, {}},
    };

    require_fragmented_round_trip(requests);
}

TEST_CASE(compat_studio_trace_completes_config_listing_and_readiness_responses) {
    const HostIdentity owner{HostTransport::tcp, 0U, 1U};
    TraceDownloadPort download_port;
    FileDownload download;
    const auto start = firmware::core::parse_file_transfer_start(
        bytes("download /sd/config.txt"));

    REQUIRE(start.has_value());
    REQUIRE(download.start(owner, start->path, 0U, download_port));
    download.handle({0xB2U, {}}, 1U, download_port);
    download.handle({0xB3U, {0U, 0U, 0U, 1U}}, 2U, download_port);
    download.handle({0xB4U, {}}, 3U, download_port);
    REQUIRE_EQ(download_port.sent[0].type, 0xB1U);
    REQUIRE_EQ(download_port.sent[1].type, 0xB2U);
    REQUIRE_EQ(download_port.sent[2].type, 0xB3U);
    REQUIRE_EQ(download_port.sent[3].type, 0xB4U);
    REQUIRE_EQ(text(download_port.sent[3].payload), std::string("ok\r\n"));

    TraceDirectoryPort directory_port;
    firmware::application::DirectoryListing::execute(
        bytes(" -e -s /sd/gcodes"), directory_port);
    REQUIRE_EQ(directory_port.sent.size(), 1U);
    REQUIRE_EQ(directory_port.sent[0].type, 0x84U);
    REQUIRE_EQ(text(directory_port.sent[0].payload),
               std::string("Load directory finished.\r\n"));

    TraceFilesystemPort filesystem_port;
    FilesystemCommands::file_type(filesystem_port);
    REQUIRE_EQ(filesystem_port.sent.size(), 1U);
    REQUIRE_EQ(filesystem_port.sent[0].type, 0x90U);
    REQUIRE_EQ(text(filesystem_port.sent[0].payload), std::string("ftype = nc\r\n"));

    ControllerSnapshots snapshots;
    snapshots.update_version(bytes("1.0.15"));
    REQUIRE_EQ(snapshots.status_reply({})->type, 0x81U);
    REQUIRE_EQ(text(snapshots.version_reply().payload),
               std::string("version = 1.0.15.0.1.11\n"));
}

TEST_CASE(compat_studio_trace_keeps_normative_missing_storage_terminals) {
    const HostIdentity owner{HostTransport::tcp, 0U, 1U};
    TraceDownloadPort download_port;
    download_port.storage_available = false;
    FileDownload download;
    const auto start = firmware::core::parse_file_transfer_start(
        bytes("download /sd/config.txt"));

    REQUIRE(start.has_value());
    REQUIRE(!download.start(owner, start->path, 0U, download_port));
    REQUIRE_EQ(download_port.sent.size(), 1U);
    REQUIRE_EQ(download_port.sent[0].type, 0xB5U);
    REQUIRE_EQ(text(download_port.sent[0].payload),
               std::string("Error: failed to get MD5 for [/sd/config.txt]!"));

    TraceDirectoryPort directory_port;
    directory_port.storage_available = false;
    firmware::application::DirectoryListing::execute(
        bytes(" -e -s /sd/gcodes"), directory_port);
    REQUIRE_EQ(directory_port.sent.size(), 1U);
    REQUIRE_EQ(directory_port.sent[0].type, 0x84U);
    REQUIRE_EQ(text(directory_port.sent[0].payload),
               std::string("Load directory finished.\r\n"));
}
