// Verifies directory-list formatting, filtering, chunking, and completion.
#include "test.hpp"

#include "firmware/application/directory_listing.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using firmware::application::DirectoryEntry;
using firmware::application::DirectoryListPort;
using firmware::application::DirectoryListing;
using firmware::application::UtcFileTime;
using firmware::core::ByteVector;
using firmware::core::Frame;

namespace {

// Converts text to the byte representation accepted by the service.
ByteVector bytes(std::string_view text) {
    return {text.begin(), text.end()};
}

// Converts a response payload to text for exact assertions.
std::string text(const ByteVector& value) {
    return {value.begin(), value.end()};
}

// Records enumeration and response operations without a real filesystem.
class FakeDirectoryListPort final : public DirectoryListPort {
public:
    // Returns the configured entries or directory-open failure.
    std::optional<std::vector<DirectoryEntry>> list_directory(
        std::string_view path) override {
        listed_path = path;
        ++list_count;
        return entries;
    }

    // Records one response frame in transmission order.
    void send(Frame frame) override {
        sent.push_back(std::move(frame));
    }

    std::optional<std::vector<DirectoryEntry>> entries =
        std::vector<DirectoryEntry>{};
    std::string listed_path;
    std::size_t list_count = 0U;
    std::vector<Frame> sent;
};

const UtcFileTime sample_time{2026U, 7U, 20U, 1U, 2U, 3U};

}  // namespace

TEST_CASE(file_011_and_012_listing_preserves_order_filters_entries_and_encodes_names) {
    FakeDirectoryListPort port;
    port.entries = std::vector<DirectoryEntry>{
        {"first file", false, 7U, sample_time, true},
        {".hidden", false, 8U, sample_time, true},
        {".md5", true, 9U, sample_time, true},
        {"unstated", false, 10U, sample_time, false},
        {"last", true, 11U, sample_time, true},
    };

    DirectoryListing::execute(bytes(" /sd"), port);

    REQUIRE_EQ(port.listed_path, std::string("/sd"));
    REQUIRE_EQ(port.sent.size(), 2U);
    REQUIRE_EQ(port.sent[0].type, 0x83U);
    REQUIRE_EQ(text(port.sent[0].payload),
               std::string("first\x01" "file\r\n.md5/\r\nlast/\r\n"));
}

TEST_CASE(file_013_detailed_listing_uses_low_size_bits_directory_zero_and_utc_time) {
    FakeDirectoryListPort port;
    port.entries = std::vector<DirectoryEntry>{
        {"file", false, 0x100000002ULL, sample_time, true},
        {"dir", true, 99U, sample_time, true},
    };

    DirectoryListing::execute(bytes(" -s /sd"), port);

    REQUIRE_EQ(text(port.sent[0].payload),
               std::string("file 2 20260720010203\r\ndir/ 0 20260720010203\r\n"));
}

TEST_CASE(file_011_lines_reaching_256_bytes_are_omitted) {
    FakeDirectoryListPort port;
    port.entries = std::vector<DirectoryEntry>{
        {std::string(253U, 'a'), false, 0U, sample_time, true},
        {std::string(254U, 'b'), false, 0U, sample_time, true},
    };

    DirectoryListing::execute(bytes("."), port);

    REQUIRE_EQ(port.sent.size(), 2U);
    REQUIRE_EQ(port.sent[0].payload.size(), 255U);
    REQUIRE_EQ(port.sent[0].payload.front(), static_cast<std::uint8_t>('a'));
}

TEST_CASE(file_014_listing_flushes_after_411_bytes) {
    FakeDirectoryListPort port;
    port.entries = std::vector<DirectoryEntry>{
        {std::string(208U, 'a'), false, 0U, sample_time, true},
        {std::string(208U, 'b'), false, 0U, sample_time, true},
        {std::string(118U, 'c'), false, 0U, sample_time, true},
    };

    DirectoryListing::execute(bytes("."), port);

    REQUIRE_EQ(port.sent.size(), 3U);
    REQUIRE_EQ(port.sent[0].payload.size(), 420U);
    REQUIRE_EQ(port.sent[1].payload.size(), 120U);
}

TEST_CASE(file_014_listing_flushes_before_a_line_would_exceed_535_bytes) {
    FakeDirectoryListPort port;
    port.entries = std::vector<DirectoryEntry>{
        {std::string(248U, 'a'), false, 0U, sample_time, true},
        {std::string(158U, 'b'), false, 0U, sample_time, true},
        {std::string(198U, 'c'), false, 0U, sample_time, true},
    };

    DirectoryListing::execute(bytes("."), port);

    REQUIRE_EQ(port.sent.size(), 3U);
    REQUIRE_EQ(port.sent[0].payload.size(), 410U);
    REQUIRE_EQ(port.sent[1].payload.size(), 200U);
}

TEST_CASE(file_015_listing_always_finishes_when_resolution_or_opening_fails) {
    FakeDirectoryListPort unavailable;
    unavailable.entries = std::nullopt;
    DirectoryListing::execute(bytes("/missing"), unavailable);

    REQUIRE_EQ(unavailable.sent.size(), 1U);
    REQUIRE_EQ(unavailable.sent[0].type, 0x84U);
    REQUIRE_EQ(text(unavailable.sent[0].payload),
               std::string("Load directory finished.\r\n"));
}
