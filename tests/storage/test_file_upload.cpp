// Verifies the host upload protocol through a deterministic fake I/O port.
#include "test.hpp"

#include "application/storage/file_upload.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

using firmware::application::FileUpload;
using firmware::application::FileUploadPort;
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

class FakeUploadPort final : public FileUploadPort {
public:
    // Records best-effort cache base preparation.
    void prepare_cache_paths(const FileCachePaths& paths) override {
        prepared_paths = paths;
    }

    // Records sidecar parent creation and its permission mode.
    bool create_parent_directories(std::string_view path, std::uint32_t mode) override {
        parent_path = path;
        parent_mode = mode;
        return parents_created;
    }

    // Records creation or truncation of the primary target.
    bool open_primary(std::string_view path) override {
        primary_path = path;
        return primary_opened;
    }

    // Records creation or truncation of the MD5 sidecar.
    bool open_md5(std::string_view path) override {
        md5_path = path;
        return md5_opened;
    }

    // Records primary data and returns configured write success.
    bool write_primary(firmware::core::BytesView data) override {
        primary_writes.emplace_back(data.begin(), data.end());
        return primary_write_succeeds;
    }

    // Records checksum bytes and returns configured write success.
    bool write_md5(firmware::core::BytesView data) override {
        md5_writes.emplace_back(data.begin(), data.end());
        return md5_write_succeeds;
    }

    // Records closing without final flush requirements.
    void close_files() override {
        ++close_count;
    }

    // Records final flush and closure after the `0xB4` acknowledgement.
    void flush_and_close() override {
        ++flush_close_count;
    }

    // Records a best-effort file removal.
    bool remove_file(std::string_view path) override {
        removed_paths.emplace_back(path);
        return remove_succeeds;
    }

    // Records firmware partial-file rename.
    bool rename_file(std::string_view source, std::string_view destination) override {
        rename_source = source;
        rename_destination = destination;
        return rename_succeeds;
    }

    // Records a response with its retained destination identity.
    void send(const HostIdentity& host, Frame frame) override {
        destinations.push_back(host);
        sent.push_back(std::move(frame));
    }

    void delay(std::uint32_t milliseconds) override {
        delays.push_back(milliseconds);
    }

    // Records logical ownership release.
    void release_ownership() override {
        ++release_count;
    }

    FileCachePaths prepared_paths;
    bool parents_created = true;
    bool primary_opened = true;
    bool md5_opened = true;
    bool primary_write_succeeds = true;
    bool md5_write_succeeds = true;
    bool remove_succeeds = true;
    bool rename_succeeds = true;
    std::string parent_path;
    std::string primary_path;
    std::string md5_path;
    std::string rename_source;
    std::string rename_destination;
    std::uint32_t parent_mode = 0U;
    std::size_t close_count = 0U;
    std::size_t flush_close_count = 0U;
    std::size_t release_count = 0U;
    std::vector<std::string> removed_paths;
    std::vector<ByteVector> primary_writes;
    std::vector<ByteVector> md5_writes;
    std::vector<HostIdentity> destinations;
    std::vector<Frame> sent;
    std::vector<std::uint32_t> delays;
};

const HostIdentity owner{HostTransport::usb, 0U, 3U};

void accept_md5(FileUpload& upload, FakeUploadPort& port, std::uint64_t now = 1U) {
    upload.handle({0xB1U, ByteVector(32U, 'a')}, now, port);
}

void accept_geometry(FileUpload& upload, FakeUploadPort& port, std::uint32_t count,
                     std::uint64_t now = 2U) {
    upload.handle({0xB2U,
                   {static_cast<std::uint8_t>(count >> 24U),
                    static_cast<std::uint8_t>(count >> 16U),
                    static_cast<std::uint8_t>(count >> 8U),
                    static_cast<std::uint8_t>(count)}},
                  now, port);
}

}  // namespace

TEST_CASE(hftu_001_upload_requires_md5_mapping_and_0777_sidecar_parents) {
    FileUpload invalid;
    FakeUploadPort invalid_port;
    REQUIRE(!invalid.start(owner, "/other/file.bin", 0U, invalid_port));
    REQUIRE_EQ(text(invalid_port.sent.back().payload), std::string("Error: Invalid filename!"));

    FileUpload valid;
    FakeUploadPort valid_port;
    REQUIRE(valid.start(owner, "/sd/file.bin", 0U, valid_port));
    REQUIRE_EQ(valid_port.parent_path, std::string("/sd/.md5/file.bin"));
    REQUIRE_EQ(valid_port.parent_mode, 0777U);
}

TEST_CASE(hftu_001_second_open_failure_leaves_primary_open_and_reports_sidecar) {
    FileUpload upload;
    FakeUploadPort port;
    port.md5_opened = false;

    REQUIRE(!upload.start(owner, "/sd/file.bin", 0U, port));

    REQUIRE_EQ(port.close_count, 0U);
    REQUIRE(port.removed_paths.empty());
    REQUIRE_EQ(text(port.sent.back().payload),
               std::string("Error: failed to open file [/sd/.md5/file.bin]!"));
}

TEST_CASE(hftu_001_parent_and_primary_open_failures_release_without_false_cleanup) {
    FileUpload parent_failure;
    FakeUploadPort parent_port;
    parent_port.parents_created = false;
    REQUIRE(!parent_failure.start(owner, "/sd/file.bin", 0U, parent_port));
    REQUIRE_EQ(parent_port.release_count, 1U);
    REQUIRE_EQ(parent_port.close_count, 0U);
    REQUIRE(parent_port.removed_paths.empty());

    FileUpload primary_failure;
    FakeUploadPort primary_port;
    primary_port.primary_opened = false;
    REQUIRE(!primary_failure.start(owner, "/sd/file.bin", 0U, primary_port));
    REQUIRE_EQ(primary_port.release_count, 1U);
    REQUIRE_EQ(primary_port.close_count, 0U);
    REQUIRE(primary_port.removed_paths.empty());
    REQUIRE_EQ(text(primary_port.sent.back().payload),
               std::string("Error: failed to open file [/sd/file.bin]!"));
}

TEST_CASE(hftu_002_firmware_upload_uses_partial_path_case_insensitively) {
    FileUpload upload;
    FakeUploadPort port;

    REQUIRE(upload.start(owner, "/sd/Firmware.BIN", 0U, port));

    REQUIRE_EQ(port.primary_path, std::string("/sd/firmware.bin.part"));
    REQUIRE_EQ(port.removed_paths.front(), std::string("/sd/firmware.bin.part"));
}

TEST_CASE(hftu_003_first_md5_packet_writes_exactly_32_bytes_and_requests_geometry) {
    FileUpload upload;
    FakeUploadPort port;
    REQUIRE(upload.start(owner, "/sd/file.bin", 0U, port));
    ByteVector payload(40U, 'a');
    payload[32] = 'z';

    upload.handle({0xB1U, payload}, 1U, port);

    REQUIRE_EQ(port.md5_writes.front().size(), 32U);
    REQUIRE_EQ(port.sent.back(), Frame({0xB2U, {}}));
}

TEST_CASE(hftu_004_geometry_announces_count_then_requests_sequence_one) {
    FileUpload upload;
    FakeUploadPort port;
    REQUIRE(upload.start(owner, "/sd/file.bin", 0U, port));
    accept_md5(upload, port);

    accept_geometry(upload, port, 3U);

    REQUIRE_EQ(port.sent.back(), Frame({0xB3U, {0U, 0U, 0U, 1U}}));
}

TEST_CASE(hftu_005_data_appends_and_requests_until_announced_count) {
    FileUpload upload;
    FakeUploadPort port;
    REQUIRE(upload.start(owner, "/sd/file.bin", 0U, port));
    accept_md5(upload, port);
    accept_geometry(upload, port, 2U);

    upload.handle({0xB3U, {0U, 0U, 0U, 1U, 'a'}}, 3U, port);
    REQUIRE_EQ(port.primary_writes.back(), ByteVector({'a'}));
    REQUIRE_EQ(port.sent.back(), Frame({0xB3U, {0U, 0U, 0U, 2U}}));

    upload.handle({0xB3U, {0U, 0U, 0U, 2U}}, 4U, port);
    REQUIRE_EQ(port.primary_writes.back(), ByteVector{});
    REQUIRE_EQ(port.sent[port.sent.size() - 2U], Frame({0xB4U, {'o', 'k', '\r', '\n'}}));
    REQUIRE_EQ(port.delays.size(), 3U);
}

TEST_CASE(hftu_005_zero_announced_count_still_accepts_sequence_one_then_finalizes) {
    FileUpload upload;
    FakeUploadPort port;
    REQUIRE(upload.start(owner, "/sd/file.bin", 0U, port));
    accept_md5(upload, port);
    accept_geometry(upload, port, 0U);

    upload.handle({0xB3U, {0U, 0U, 0U, 1U}}, 3U, port);

    REQUIRE(!upload.active());
    REQUIRE_EQ(port.flush_close_count, 1U);
}

TEST_CASE(hftu_006_write_failure_retains_sequence_and_sends_exact_retry_error) {
    FileUpload upload;
    FakeUploadPort port;
    REQUIRE(upload.start(owner, "/sd/file.bin", 0U, port));
    accept_md5(upload, port);
    accept_geometry(upload, port, 2U);
    port.primary_write_succeeds = false;

    upload.handle({0xB3U, {0U, 0U, 0U, 1U, 'x'}}, 3U, port);

    REQUIRE_EQ(port.sent.back().type, 0xB6U);
    REQUIRE_EQ(text(port.sent.back().payload), std::string("Error: File Write error!retry..."));
    REQUIRE(upload.active());
    REQUIRE_EQ(port.delays.back(), 10U);
}

TEST_CASE(hftu_007_ack_precedes_flush_and_success_release) {
    FileUpload upload;
    FakeUploadPort port;
    REQUIRE(upload.start(owner, "/sd/file.bin", 0U, port));
    accept_md5(upload, port);
    accept_geometry(upload, port, 1U);

    upload.handle({0xB3U, {0U, 0U, 0U, 1U, 'x'}}, 3U, port);

    REQUIRE_EQ(port.sent[port.sent.size() - 2U].type, 0xB4U);
    REQUIRE_EQ(port.flush_close_count, 1U);
    REQUIRE_EQ(port.sent.back().type, 0x90U);
    REQUIRE_EQ(text(port.sent.back().payload), std::string("Info: upload success: /sd/file.bin."));
    REQUIRE_EQ(port.release_count, 1U);
}

TEST_CASE(hftu_008_firmware_rename_failure_occurs_after_ack_and_removes_partial) {
    FileUpload upload;
    FakeUploadPort port;
    port.rename_succeeds = false;
    REQUIRE(upload.start(owner, "/sd/firmware.bin", 0U, port));
    accept_md5(upload, port);
    accept_geometry(upload, port, 1U);

    upload.handle({0xB3U, {0U, 0U, 0U, 1U}}, 3U, port);

    REQUIRE_EQ(port.rename_source, std::string("/sd/firmware.bin.part"));
    REQUIRE_EQ(port.rename_destination, std::string("/sd/firmware.bin"));
    REQUIRE_EQ(port.sent[port.sent.size() - 2U].type, 0xB4U);
    REQUIRE_EQ(port.sent.back().type, 0xB5U);
    REQUIRE_EQ(text(port.sent.back().payload),
               std::string("Error: failed to finalize firmware upload [/sd/firmware.bin]."));
}

TEST_CASE(hftu_010_cancel_removes_target_and_md5_then_releases) {
    FileUpload upload;
    FakeUploadPort port;
    REQUIRE(upload.start(owner, "/sd/file.bin", 0U, port));

    upload.handle({0xB5U, {}}, 1U, port);

    REQUIRE_EQ(text(port.sent.back().payload),
               std::string("Info: Upload canceled by remote!"));
    REQUIRE_EQ(port.removed_paths.size(), 2U);
    REQUIRE_EQ(port.release_count, 1U);
    REQUIRE(port.delays.empty());
}

TEST_CASE(hft_022_timed_retry_uses_501_silent_intervals_and_an_extra_delay) {
    FileUpload upload;
    FakeUploadPort port;
    REQUIRE(upload.start(owner, "/sd/file.bin", 0U, port));

    for (std::size_t interval = 0U; interval < 500U; ++interval) {
        upload.poll((interval + 1U) * 10U, port);
    }
    REQUIRE(port.sent.empty());
    upload.poll(5010U, port);
    REQUIRE_EQ(port.sent.back(), Frame({0xB6U, bytes("Info: need retry!")}));
    REQUIRE_EQ(port.delays.size(), 502U);
}

TEST_CASE(hftu_011_delays_every_nonterminal_cycle_but_not_completion_or_cancel) {
    FileUpload upload;
    FakeUploadPort port;
    REQUIRE(upload.start(owner, "/sd/file.bin", 0U, port));

    accept_md5(upload, port);
    accept_geometry(upload, port, 2U);
    upload.handle({0xBFU, {}}, 3U, port);
    port.primary_write_succeeds = false;
    upload.handle({0xB3U, {0U, 0U, 0U, 1U, 'x'}}, 4U, port);
    port.primary_write_succeeds = true;
    upload.handle({0xB3U, {0U, 0U, 0U, 1U, 'x'}}, 5U, port);
    REQUIRE_EQ(port.delays, std::vector<std::uint32_t>({10U, 10U, 10U, 10U, 10U}));

    upload.handle({0xB3U, {0U, 0U, 0U, 2U}}, 6U, port);
    REQUIRE_EQ(port.delays.size(), 5U);

    FileUpload cancelled;
    FakeUploadPort cancelled_port;
    REQUIRE(cancelled.start(owner, "/sd/file.bin", 0U, cancelled_port));
    cancelled.handle({0xB5U, {}}, 1U, cancelled_port);
    REQUIRE(cancelled_port.delays.empty());
}

TEST_CASE(hft_020_inactive_operations_are_ignored_and_expired_resume_aborts) {
    FileUpload inactive;
    FakeUploadPort inactive_port;
    inactive.handle({0xB1U, ByteVector(32U, 'a')}, 1U, inactive_port);
    inactive.poll(10000U, inactive_port);
    inactive.resume(10000U, inactive_port);
    REQUIRE(inactive_port.sent.empty());

    FileUpload expired;
    FakeUploadPort expired_port;
    REQUIRE(expired.start(owner, "/sd/file.bin", 0U, expired_port));
    expired.resume(9001U, expired_port);
    REQUIRE(!expired.active());
    REQUIRE_EQ(text(expired_port.sent.back().payload),
               std::string("Info: Machine receive file time out!"));
}

TEST_CASE(own_008_reconnected_upload_repeats_the_unverified_sequence) {
    FileUpload upload;
    FakeUploadPort first_connection;
    REQUIRE(upload.start(owner, "/sd/file.bin", 0U, first_connection));
    accept_md5(upload, first_connection);
    accept_geometry(upload, first_connection, 3U);
    upload.handle({0xB3U, {0U, 0U, 0U, 1U, 'a'}}, 3U,
                  first_connection);

    FakeUploadPort reconnected;
    upload.resume(4U, reconnected);

    REQUIRE(upload.active());
    REQUIRE_EQ(reconnected.sent.size(), 1U);
    REQUIRE_EQ(reconnected.sent.front(),
               Frame({0xB3U, {0U, 0U, 0U, 2U}}));
}

TEST_CASE(hft_021_upload_timeout_uses_exact_message_after_more_than_nine_seconds) {
    FileUpload upload;
    FakeUploadPort port;
    REQUIRE(upload.start(owner, "/sd/file.bin", 100U, port));

    upload.poll(9100U, port);
    REQUIRE(upload.active());
    upload.poll(9101U, port);

    REQUIRE(!upload.active());
    REQUIRE_EQ(text(port.sent.back().payload),
               std::string("Info: Machine receive file time out!"));
}

TEST_CASE(hft_024_fifty_one_wrong_packets_repeat_current_request) {
    FileUpload upload;
    FakeUploadPort port;
    REQUIRE(upload.start(owner, "/sd/file.bin", 0U, port));

    for (std::size_t count = 0U; count < 51U; ++count) {
        upload.handle({0xB2U, {}}, count + 1U, port);
    }

    REQUIRE(upload.active());
    REQUIRE_EQ(port.sent.back(), Frame({0xB1U, {}}));
}

TEST_CASE(hft_024_wrong_packets_repeat_geometry_and_abort_after_maximum_cycles) {
    FileUpload geometry;
    FakeUploadPort geometry_port;
    REQUIRE(geometry.start(owner, "/sd/file.bin", 0U, geometry_port));
    accept_md5(geometry, geometry_port);
    for (std::size_t count = 0U; count < 51U; ++count) {
        geometry.handle({0xBFU, {}}, count + 2U, geometry_port);
    }
    REQUIRE_EQ(geometry_port.sent.back(), Frame({0xB2U, {}}));

    FileUpload excessive;
    FakeUploadPort excessive_port;
    REQUIRE(excessive.start(owner, "/sd/file.bin", 0U, excessive_port));
    for (std::size_t count = 0U; count < 51U * 51U; ++count) {
        excessive.handle({0xBFU, {}}, count + 1U, excessive_port);
    }
    REQUIRE(!excessive.active());
    REQUIRE_EQ(excessive_port.sent[excessive_port.sent.size() - 2U],
               Frame({0xB1U, {}}));
    REQUIRE_EQ(text(excessive_port.sent.back().payload),
               std::string("Info: Machine receive file too many retry error!"));
}

TEST_CASE(hft_024_and_025_histories_cross_metadata_but_nonfinal_data_resets_them) {
    FileUpload retained;
    FakeUploadPort retained_port;
    REQUIRE(retained.start(owner, "/sd/file.bin", 0U, retained_port));
    for (std::size_t count = 0U; count < 50U; ++count) {
        retained.handle({0xBFU, {}}, count + 1U, retained_port);
    }
    accept_md5(retained, retained_port, 51U);
    accept_geometry(retained, retained_port, 2U, 52U);
    retained.handle({0xBFU, {}}, 53U, retained_port);
    REQUIRE_EQ(retained_port.sent.back(),
               Frame({0xB3U, {0U, 0U, 0U, 1U}}));

    FileUpload reset;
    FakeUploadPort reset_port;
    REQUIRE(reset.start(owner, "/sd/file.bin", 0U, reset_port));
    accept_md5(reset, reset_port);
    accept_geometry(reset, reset_port, 2U);
    for (std::size_t count = 0U; count < 50U; ++count) {
        reset.handle({0xBFU, {}}, count + 3U, reset_port);
    }
    reset.handle({0xB3U, {0U, 0U, 0U, 1U, 'x'}}, 53U, reset_port);
    const std::size_t sent_after_data = reset_port.sent.size();
    for (std::size_t count = 0U; count < 50U; ++count) {
        reset.handle({0xBFU, {}}, count + 54U, reset_port);
    }
    REQUIRE_EQ(reset_port.sent.size(), sent_after_data);
    reset.handle({0xBFU, {}}, 104U, reset_port);
    REQUIRE_EQ(reset_port.sent.back(),
               Frame({0xB3U, {0U, 0U, 0U, 2U}}));
}
