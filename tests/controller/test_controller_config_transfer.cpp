// Verifies controller configuration transfer filtering and aggregation.
#include "test.hpp"

#include "application/controller/controller_config_transfer.hpp"

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

using firmware::application::ControllerConfigPort;
using firmware::application::ControllerConfigTransfer;
using firmware::core::ByteVector;
using firmware::core::Frame;

namespace {

ByteVector bytes(std::string_view text) {
    return {text.begin(), text.end()};
}

class FakeConfigPort final : public ControllerConfigPort {
public:
    // Reports configured presence and records the requested path.
    bool configuration_available() override {
        return exists;
    }

    // Returns configured 255-byte input chunks and records the requested path.
    std::optional<std::vector<ByteVector>> read_configuration_chunks(
        std::size_t chunk_size) override {
        requested_chunk_size = chunk_size;
        return chunks;
    }

    // Records one response and reports configured submission success.
    bool send(Frame frame) override {
        sent.push_back(std::move(frame));
        return send_succeeds;
    }

    void diagnose(firmware::application::ControllerTransferDiagnostic diagnostic) override {
        diagnostics.push_back(std::move(diagnostic));
    }

    bool exists = true;
    bool send_succeeds = true;
    std::size_t requested_chunk_size = 0U;
    std::optional<std::vector<ByteVector>> chunks = std::vector<ByteVector>{};
    std::vector<Frame> sent;
    std::vector<firmware::application::ControllerTransferDiagnostic> diagnostics;
};

Frame geometry() {
    return {0xD2U, {0U, 0U, 0U, 99U, 0U, 1U}};
}

}  // namespace

TEST_CASE(lpccfg_001_start_acknowledges_before_reporting_an_absent_exact_path) {
    ControllerConfigTransfer transfer;
    FakeConfigPort port;
    port.exists = false;

    transfer.handle({0xD1U, {}}, port);

    REQUIRE_EQ(port.sent.size(), 2U);
    REQUIRE_EQ(port.sent[0].type, 0xD1U);
    REQUIRE_EQ(port.sent[1].type, 0xD5U);
    REQUIRE(!transfer.active());
}

TEST_CASE(lpccfg_001_available_start_activates_controller_traffic_suppression) {
    ControllerConfigTransfer transfer;
    FakeConfigPort port;

    transfer.handle({0xD1U, {}}, port);

    REQUIRE(transfer.active());
    REQUIRE_EQ(port.sent.back().type, 0xD1U);
    REQUIRE_EQ(port.diagnostics.back().message,
               std::string("Received PTYPE_CONFIG_START"));
}

TEST_CASE(lpccfg_002_geometry_counts_all_chunks_longer_than_two_including_comments) {
    ControllerConfigTransfer transfer;
    FakeConfigPort port;
    port.chunks = std::vector<ByteVector>{bytes("#comment\n"), bytes("x\n"), bytes("abc\n")};

    transfer.handle(geometry(), port);

    REQUIRE_EQ(port.requested_chunk_size, 255U);
    REQUIRE_EQ(port.sent.back(), Frame({0xD2U, {0U, 0U, 0U, 2U, 2U, 0U}}));
    REQUIRE_EQ(transfer.frame_count(), 2U);
    REQUIRE_EQ(transfer.frame_data_size(), 512U);
}

TEST_CASE(lpccfg_003_data_filters_comments_stars_and_nonfinal_chunks_without_lf) {
    ControllerConfigTransfer transfer;
    FakeConfigPort port;
    port.chunks = std::vector<ByteVector>{
        bytes("#comment\n"),
        bytes("*directive\n"),
        bytes("incomplete"),
        bytes("first\n"),
        bytes("final"),
    };
    transfer.handle(geometry(), port);

    transfer.handle({0xD3U, {0U, 0U, 0U, 1U}}, port);

    REQUIRE_EQ(port.sent.back().payload,
               ByteVector({0U, 0U, 0U, 1U, 'f', 'i', 'r', 's', 't', '\n', 'f', 'i', 'n', 'a',
                           'l', '\n'}));
}

TEST_CASE(lpccfg_003_index_two_skips_two_eligible_records) {
    ControllerConfigTransfer transfer;
    FakeConfigPort port;
    port.chunks = std::vector<ByteVector>{bytes("one\n"), bytes("two\n"), bytes("three\n")};
    transfer.handle(geometry(), port);

    transfer.handle({0xD3U, {0U, 0U, 0U, 2U}}, port);

    REQUIRE_EQ(port.sent.back().payload,
               ByteVector({0U, 0U, 0U, 2U, 't', 'h', 'r', 'e', 'e', '\n', '\n'}));
}

TEST_CASE(lpccfg_004_long_records_use_first_62_bytes_then_lf_and_nul) {
    ControllerConfigTransfer transfer;
    FakeConfigPort port;
    port.chunks = std::vector<ByteVector>{ByteVector(65U, 'a')};
    transfer.handle(geometry(), port);

    transfer.handle({0xD3U, {0U, 0U, 0U, 1U}}, port);

    const auto& payload = port.sent.back().payload;
    REQUIRE_EQ(payload.size(), 69U);
    REQUIRE_EQ(payload[65], static_cast<std::uint8_t>('a'));
    REQUIRE_EQ(payload[66], static_cast<std::uint8_t>('\n'));
    REQUIRE_EQ(payload[67], 0U);
    REQUIRE_EQ(payload[68], static_cast<std::uint8_t>('\n'));
}

TEST_CASE(lpccfg_005_aggregation_stops_after_a_record_when_under_80_bytes_remain) {
    ControllerConfigTransfer transfer;
    FakeConfigPort port;
    port.chunks = std::vector<ByteVector>(8U, ByteVector(64U, 'x'));
    for (auto& chunk : *port.chunks) {
        chunk.back() = '\n';
    }
    transfer.handle(geometry(), port);

    transfer.handle({0xD3U, {0U, 0U, 0U, 1U}}, port);

    REQUIRE_EQ(port.sent.back().payload.size(), 453U);
    REQUIRE_EQ(port.sent.back().payload.back(), static_cast<std::uint8_t>('\n'));
}

TEST_CASE(lpccfg_006_terminal_packet_ends_transfer_and_there_is_no_idle_timeout) {
    ControllerConfigTransfer transfer;
    FakeConfigPort port;
    transfer.handle({0xD1U, {}}, port);

    transfer.handle({0xDFU, {}}, port);
    REQUIRE(transfer.active());
    transfer.handle({0xD5U, {}}, port);
    REQUIRE(!transfer.active());
    REQUIRE_EQ(transfer.frame_data_size(), 0U);
}

TEST_CASE(lpccfg_001_start_send_failure_reports_cancel_but_stays_active) {
    ControllerConfigTransfer transfer;
    FakeConfigPort port;
    port.send_succeeds = false;

    transfer.handle({0xD1U, {}}, port);

    REQUIRE(transfer.active());
    REQUIRE_EQ(port.sent.size(), 2U);
    REQUIRE_EQ(port.sent.back().type, 0xD5U);
}

TEST_CASE(lpccfg_002_geometry_rejects_malformed_read_and_send_failures) {
    ControllerConfigTransfer transfer;
    FakeConfigPort port;

    transfer.handle({0xD2U, {1U}}, port);
    REQUIRE_EQ(port.sent.back().type, 0xD5U);
    REQUIRE_EQ(port.diagnostics.back().message,
               std::string("Received PTYPE_CONFIG_VIEW"));
    port.chunks = std::nullopt;
    transfer.handle(geometry(), port);
    REQUIRE_EQ(port.sent.back().type, 0xD5U);
    REQUIRE_EQ(port.diagnostics[port.diagnostics.size() - 2U].message,
               std::string("Failed to open firmware file: /sd/config.txt"));
    REQUIRE_EQ(port.diagnostics.back().message,
               std::string("Received PTYPE_CONFIG_VIEW"));
    port.chunks = std::vector<ByteVector>{bytes("abc\n")};
    port.send_succeeds = false;
    transfer.handle(geometry(), port);
    REQUIRE_EQ(port.sent.back().type, 0xD5U);
}

TEST_CASE(lpccfg_003_data_rejects_malformed_unconfigured_and_io_failures) {
    ControllerConfigTransfer transfer;
    FakeConfigPort port;

    transfer.handle({0xD3U, {1U}}, port);
    REQUIRE_EQ(port.sent.back().type, 0xD5U);
    REQUIRE_EQ(port.diagnostics[port.diagnostics.size() - 2U].message,
               std::string("Received PTYPE_CONFIG_DATA"));
    const std::size_t before_unconfigured = port.sent.size();
    transfer.handle({0xD3U, {0U, 0U, 0U, 1U}}, port);
    REQUIRE_EQ(port.sent.size(), before_unconfigured);

    transfer.handle(geometry(), port);
    port.chunks = std::nullopt;
    transfer.handle({0xD3U, {0U, 0U, 0U, 1U}}, port);
    REQUIRE_EQ(port.sent.back().type, 0xD5U);
    port.chunks = std::vector<ByteVector>{bytes("#none\n")};
    const std::size_t before_empty = port.sent.size();
    transfer.handle({0xD3U, {0U, 0U, 0U, 1U}}, port);
    REQUIRE_EQ(port.sent.size(), before_empty);
    port.chunks = std::vector<ByteVector>{bytes("value\n")};
    port.send_succeeds = false;
    transfer.handle({0xD3U, {0U, 0U, 0U, 1U}}, port);
    REQUIRE_EQ(port.sent.back().type, 0xD5U);
}
