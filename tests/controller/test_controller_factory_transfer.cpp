// Verifies controller factory-data selection, limits, and completion cleanup.
#include "test.hpp"

#include "application/controller/controller_factory_transfer.hpp"

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

using firmware::application::ControllerFactoryPort;
using firmware::application::ControllerFactoryTransfer;
using firmware::core::ByteVector;
using firmware::core::Frame;

namespace {

ByteVector bytes(std::string_view text) {
    return {text.begin(), text.end()};
}

class FakeFactoryPort final : public ControllerFactoryPort {
public:
    // Reports configured presence and records the exact path.
    bool file_exists(std::string_view path) override {
        last_path = path;
        return exists;
    }

    // Returns configured fixed-size chunks and records the chunk size.
    std::optional<std::vector<ByteVector>> read_chunks(std::string_view path,
                                                       std::size_t chunk_size) override {
        last_path = path;
        requested_chunk_size = chunk_size;
        return chunks;
    }

    // Records one removal attempt and reports its configured result.
    bool remove_file(std::string_view path) override {
        last_path = path;
        ++remove_count;
        return remove_succeeds;
    }

    // Records one response and reports configured queue acceptance.
    bool send(Frame frame) override {
        sent.push_back(std::move(frame));
        return send_succeeds;
    }

    void diagnose(firmware::application::ControllerTransferDiagnostic diagnostic) override {
        diagnostics.push_back(std::move(diagnostic));
    }

    bool exists = true;
    bool remove_succeeds = true;
    bool send_succeeds = true;
    std::string_view last_path;
    std::size_t requested_chunk_size = 0U;
    std::size_t remove_count = 0U;
    std::optional<std::vector<ByteVector>> chunks = std::vector<ByteVector>{};
    std::vector<Frame> sent;
    std::vector<firmware::application::ControllerTransferDiagnostic> diagnostics;
};

Frame geometry(std::uint16_t data_size) {
    return {0xE2U, {0U, 0U, 0U, 99U, static_cast<std::uint8_t>(data_size >> 8U),
                    static_cast<std::uint8_t>(data_size)}};
}

}  // namespace

TEST_CASE(lpcfac_001_start_acknowledges_then_reports_an_absent_exact_path) {
    ControllerFactoryTransfer transfer;
    FakeFactoryPort port;
    port.exists = false;

    transfer.handle({0xE1U, {}}, port);

    REQUIRE_EQ(port.last_path, std::string_view("/sd/factory.ini"));
    REQUIRE_EQ(port.sent.size(), 2U);
    REQUIRE_EQ(port.sent[0].type, 0xE1U);
    REQUIRE_EQ(port.sent[1].type, 0xE5U);
    REQUIRE(!transfer.active());
    REQUIRE_EQ(port.diagnostics.size(), 1U);
    REQUIRE_EQ(port.diagnostics.front().message,
               std::string("Factory ini file does not exist: /sd/factory.ini"));
}

TEST_CASE(diag_034_factory_overlong_record_reports_located_data_before_rejection) {
    ControllerFactoryTransfer transfer;
    FakeFactoryPort port;
    port.chunks = std::vector<ByteVector>{ByteVector(133U, 'x')};
    transfer.handle(geometry(132U), port);
    const std::size_t replies_before = port.sent.size();

    transfer.handle({0xE3U, {0U, 0U, 0U, 1U}}, port);

    REQUIRE_EQ(port.sent.size(), replies_before);
    REQUIRE_EQ(port.diagnostics[port.diagnostics.size() - 3U].message,
               std::string("Received PTYPE_FACTORY_DATA"));
    REQUIRE_EQ(port.diagnostics[port.diagnostics.size() - 2U].message,
               std::string("Received device request for frame 1 data"));
    REQUIRE_EQ(port.diagnostics.back().message,
               std::string("Frame 1 data sent successfully"));
}

TEST_CASE(lpcfac_002_geometry_counts_stars_but_excludes_hash_comments) {
    ControllerFactoryTransfer transfer;
    FakeFactoryPort port;
    port.chunks = std::vector<ByteVector>{bytes("#comment"), bytes("*value"), bytes("ab"),
                                          bytes("plain")};

    transfer.handle(geometry(100U), port);

    REQUIRE_EQ(port.requested_chunk_size, 255U);
    REQUIRE_EQ(port.sent.back(), Frame({0xE2U, {0U, 0U, 0U, 2U, 0U, 100U}}));
    REQUIRE_EQ(transfer.frame_count(), 2U);
    REQUIRE_EQ(transfer.frame_data_size(), 100U);
}

TEST_CASE(lpcfac_002_geometry_rejects_zero_and_more_than_512_without_clearing_old_geometry) {
    ControllerFactoryTransfer transfer;
    FakeFactoryPort port;
    transfer.handle(geometry(100U), port);

    transfer.handle(geometry(0U), port);
    REQUIRE_EQ(port.sent.back().type, 0xE5U);
    REQUIRE_EQ(transfer.frame_data_size(), 100U);
    transfer.handle(geometry(513U), port);
    REQUIRE_EQ(port.sent.back().type, 0xE5U);
}

TEST_CASE(lpcfac_003_index_zero_and_missing_or_oversized_records_produce_no_reply) {
    ControllerFactoryTransfer transfer;
    FakeFactoryPort port;
    port.chunks = std::vector<ByteVector>{ByteVector(133U, 'x')};
    transfer.handle(geometry(132U), port);
    const std::size_t before = port.sent.size();

    transfer.handle({0xE3U, {0U, 0U, 0U, 0U}}, port);
    transfer.handle({0xE3U, {0U, 0U, 0U, 1U}}, port);
    transfer.handle({0xE3U, {0U, 0U, 0U, 2U}}, port);

    REQUIRE_EQ(port.sent.size(), before);
}

TEST_CASE(lpcfac_003_positive_index_selects_the_nth_eligible_record) {
    ControllerFactoryTransfer transfer;
    FakeFactoryPort port;
    port.chunks = std::vector<ByteVector>{bytes("#skip"), bytes("first"), bytes("*second")};
    transfer.handle(geometry(100U), port);

    transfer.handle({0xE3U, {0U, 0U, 0U, 2U}}, port);

    REQUIRE_EQ(port.sent.back(),
               Frame({0xE3U, {0U, 0U, 0U, 2U, '*', 's', 'e', 'c', 'o', 'n', 'd'}}));
}

TEST_CASE(lpcfac_004_completion_removes_once_even_when_inactive_and_ignores_failure) {
    ControllerFactoryTransfer transfer;
    FakeFactoryPort port;
    port.remove_succeeds = false;

    transfer.handle({0xE4U, {}}, port);

    REQUIRE_EQ(port.remove_count, 1U);
    REQUIRE_EQ(port.last_path, std::string_view("/sd/factory.ini"));
    REQUIRE(port.sent.empty());
}

TEST_CASE(lpcfac_004_cancellation_does_not_remove_the_factory_file) {
    ControllerFactoryTransfer transfer;
    FakeFactoryPort port;
    transfer.handle({0xE1U, {}}, port);
    REQUIRE(transfer.active());

    transfer.handle({0xE5U, {}}, port);

    REQUIRE(!transfer.active());
    REQUIRE_EQ(port.remove_count, 0U);
    REQUIRE_EQ(transfer.frame_data_size(), 0U);
}

TEST_CASE(lpcfac_001_start_send_failure_reports_cancel_but_stays_active) {
    ControllerFactoryTransfer transfer;
    FakeFactoryPort port;
    port.send_succeeds = false;

    transfer.handle({0xE1U, {}}, port);

    REQUIRE(transfer.active());
    REQUIRE_EQ(port.sent.size(), 2U);
    REQUIRE_EQ(port.sent.back().type, 0xE5U);
}

TEST_CASE(lpcfac_002_geometry_rejects_malformed_read_and_send_failures) {
    ControllerFactoryTransfer transfer;
    FakeFactoryPort port;

    transfer.handle({0xE2U, {1U}}, port);
    REQUIRE_EQ(port.sent.back().type, 0xE5U);
    port.chunks = std::nullopt;
    transfer.handle(geometry(100U), port);
    REQUIRE_EQ(port.sent.back().type, 0xE5U);
    REQUIRE_EQ(port.diagnostics[port.diagnostics.size() - 2U].message,
               std::string("Failed to open Factory ini file: /sd/factory.ini"));
    REQUIRE_EQ(port.diagnostics.back().message,
               std::string("Received PTYPE_FACTORY_VIEW"));
    port.chunks = std::vector<ByteVector>{bytes("value")};
    port.send_succeeds = false;
    transfer.handle(geometry(100U), port);
    REQUIRE_EQ(port.sent.back().type, 0xE5U);
}

TEST_CASE(lpcfac_003_data_rejects_malformed_unconfigured_and_io_failures) {
    ControllerFactoryTransfer transfer;
    FakeFactoryPort port;

    transfer.handle({0xE3U, {1U}}, port);
    REQUIRE_EQ(port.sent.back().type, 0xE5U);
    const std::size_t before_unconfigured = port.sent.size();
    transfer.handle({0xE3U, {0U, 0U, 0U, 1U}}, port);
    REQUIRE_EQ(port.sent.size(), before_unconfigured);

    transfer.handle(geometry(100U), port);
    port.chunks = std::nullopt;
    transfer.handle({0xE3U, {0U, 0U, 0U, 1U}}, port);
    REQUIRE_EQ(port.sent.back().type, 0xE5U);
    port.chunks = std::vector<ByteVector>{bytes("value")};
    port.send_succeeds = false;
    transfer.handle({0xE3U, {0U, 0U, 0U, 1U}}, port);
    REQUIRE_EQ(port.sent.back().type, 0xE5U);
}
