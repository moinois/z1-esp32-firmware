// Verifies the controller firmware-transfer state machine through a fake port.
#include "test.hpp"

#include "application/controller/controller_firmware_transfer.hpp"

#include <optional>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

using firmware::application::ControllerFirmwarePort;
using firmware::application::ControllerFirmwareTransfer;
using firmware::application::FirmwareTransferEvent;
using firmware::core::ByteVector;
using firmware::core::Frame;

namespace {

class FakeFirmwarePort final : public ControllerFirmwarePort {
public:
    // Reports the configured file-presence result and records the exact path.
    bool file_exists(std::string_view path) override {
        last_path = path;
        return exists;
    }

    // Reports the configured file size and records the exact path.
    std::optional<std::uint64_t> file_size(std::string_view path) override {
        last_path = path;
        return size;
    }

    // Records one content request and returns the configured read result.
    std::optional<ByteVector> read_file(std::string_view path, std::uint64_t offset,
                                       std::size_t maximum_size) override {
        last_path = path;
        read_offset = offset;
        read_maximum = maximum_size;
        ++read_count;
        return read_result;
    }

    bool response_data_memory_available(std::size_t bytes) override {
        allocation_size = bytes;
        return allocation_succeeds;
    }

    // Records one response and reports whether its submission succeeded.
    bool send(Frame frame) override {
        if (sent.empty()) diagnostics_at_first_send = diagnostics.size();
        sent.push_back(std::move(frame));
        return send_succeeds;
    }

    void diagnose(firmware::application::ControllerTransferDiagnostic diagnostic) override {
        diagnostics.push_back(std::move(diagnostic));
    }

    // Records one externally observable update-state transition.
    void publish(FirmwareTransferEvent event, std::uint32_t index,
                 std::uint32_t frame_count) override {
        events.push_back(event);
        progress_index = index;
        progress_count = frame_count;
    }

    bool exists = true;
    bool send_succeeds = true;
    bool allocation_succeeds = true;
    std::size_t allocation_size = 0U;
    std::optional<std::uint64_t> size = 1025U;
    std::optional<ByteVector> read_result = ByteVector({1U, 2U});
    std::string_view last_path;
    std::uint64_t read_offset = 0U;
    std::size_t read_maximum = 0U;
    std::size_t read_count = 0U;
    std::size_t diagnostics_at_first_send = 0U;
    std::uint32_t progress_index = 0U;
    std::uint32_t progress_count = 0U;
    std::vector<Frame> sent;
    std::vector<FirmwareTransferEvent> events;
    std::vector<firmware::application::ControllerTransferDiagnostic> diagnostics;
};

Frame geometry(std::uint32_t proposed_count, std::uint16_t data_size) {
    return {
        0xC2U,
        {
            static_cast<std::uint8_t>(proposed_count >> 24U),
            static_cast<std::uint8_t>(proposed_count >> 16U),
            static_cast<std::uint8_t>(proposed_count >> 8U),
            static_cast<std::uint8_t>(proposed_count),
            static_cast<std::uint8_t>(data_size >> 8U),
            static_cast<std::uint8_t>(data_size),
        },
    };
}

}  // namespace

TEST_CASE(lpcfw_001_start_uses_the_exact_path_and_rejects_an_absent_file) {
    ControllerFirmwareTransfer transfer;
    FakeFirmwarePort port;
    port.exists = false;

    transfer.handle({0xC1U, {}}, 0U, port);

    REQUIRE_EQ(port.last_path, std::string_view("/sd/lpc1768.bin"));
    REQUIRE_EQ(port.sent.back().type, 0xC5U);
    REQUIRE(!transfer.active());
}

TEST_CASE(lpcfw_001_available_start_activates_suppression_and_publishes_start) {
    ControllerFirmwareTransfer transfer;
    FakeFirmwarePort port;

    transfer.handle({0xC1U, {}}, 100U, port);

    REQUIRE_EQ(port.sent.back(), Frame({0xC1U, {}}));
    REQUIRE(transfer.active());
    REQUIRE_EQ(port.events.back(), FirmwareTransferEvent::started);
    REQUIRE_EQ(port.diagnostics_at_first_send, 1U);
    REQUIRE_EQ(port.diagnostics.front().message,
               std::string("Received PTYPE_FIRM_START"));
}

TEST_CASE(diag_035_firmware_data_retention_failure_is_explicit_and_cancels) {
    ControllerFirmwareTransfer transfer;
    FakeFirmwarePort port;
    transfer.handle({0xC1U, {}}, 0U, port);
    transfer.handle(geometry(1U, 512U), 1U, port);
    port.allocation_succeeds = false;

    transfer.handle({0xC3U, {0U, 0U, 0U, 1U}}, 2U, port);

    REQUIRE_EQ(port.allocation_size, 6U);
    REQUIRE_EQ(port.diagnostics.back().message,
               std::string("Failed to allocate memory for frame data"));
    REQUIRE_EQ(port.sent.back().type, 0xC5U);
}

TEST_CASE(lpcfw_002_geometry_ignores_proposed_count_and_rounds_up_file_blocks) {
    ControllerFirmwareTransfer transfer;
    FakeFirmwarePort port;
    transfer.handle({0xC1U, {}}, 0U, port);

    transfer.handle(geometry(99U, 512U), 1U, port);

    REQUIRE_EQ(port.sent.back(), Frame({0xC2U, {0U, 0U, 0U, 3U, 2U, 0U}}));
    REQUIRE_EQ(transfer.frame_count(), 3U);
    REQUIRE_EQ(transfer.frame_data_size(), 512U);
}

TEST_CASE(lpc_013_bad_firmware_geometry_sends_error_without_ending_transfer) {
    ControllerFirmwareTransfer transfer;
    FakeFirmwarePort port;
    transfer.handle({0xC1U, {}}, 0U, port);

    transfer.handle(geometry(1U, 0U), 1U, port);

    REQUIRE_EQ(port.sent.back().type, 0xC5U);
    REQUIRE_EQ(port.events.back(), FirmwareTransferEvent::error);
    REQUIRE(transfer.active());
}

TEST_CASE(lpcfw_003_indexes_zero_and_one_read_the_first_block) {
    ControllerFirmwareTransfer transfer;
    FakeFirmwarePort port;
    transfer.handle(geometry(0U, 100U), 0U, port);

    transfer.handle({0xC3U, {0U, 0U, 0U, 1U}}, 1U, port);

    REQUIRE_EQ(port.read_offset, 0U);
    REQUIRE_EQ(port.read_maximum, 100U);
    REQUIRE_EQ(port.sent.back(), Frame({0xC3U, {0U, 0U, 0U, 1U, 1U, 2U}}));
}

TEST_CASE(lpcfw_003_later_indexes_reopen_at_index_minus_one_blocks) {
    ControllerFirmwareTransfer transfer;
    FakeFirmwarePort port;
    transfer.handle(geometry(0U, 100U), 0U, port);

    transfer.handle({0xC3U, {0U, 0U, 0U, 4U}}, 1U, port);

    REQUIRE_EQ(port.read_offset, 300U);
    REQUIRE_EQ(port.last_path, std::string_view("/sd/lpc1768.bin"));
}

TEST_CASE(lpc_015_successful_zero_byte_read_produces_no_data_reply) {
    ControllerFirmwareTransfer transfer;
    FakeFirmwarePort port;
    port.read_result = ByteVector{};
    transfer.handle(geometry(0U, 100U), 0U, port);
    const std::size_t sent_before = port.sent.size();

    transfer.handle({0xC3U, {0U, 0U, 0U, 1U}}, 1U, port);

    REQUIRE_EQ(port.sent.size(), sent_before);
    REQUIRE_EQ(port.diagnostics[port.diagnostics.size() - 2U].message,
               std::string("Received device request for frame 1 data"));
    REQUIRE_EQ(port.diagnostics.back().message,
               std::string("Frame 1 data sent successfully"));
}

TEST_CASE(lpcfw_004_sent_block_publishes_requested_index_and_negotiated_count) {
    ControllerFirmwareTransfer transfer;
    FakeFirmwarePort port;
    transfer.handle(geometry(0U, 512U), 0U, port);

    transfer.handle({0xC3U, {0U, 0U, 0U, 2U}}, 1U, port);

    REQUIRE_EQ(port.events.back(), FirmwareTransferEvent::progress);
    REQUIRE_EQ(port.progress_index, 2U);
    REQUIRE_EQ(port.progress_count, 3U);
}

TEST_CASE(lpc_016_terminal_packets_end_even_an_inactive_transfer_and_clear_geometry) {
    ControllerFirmwareTransfer transfer;
    FakeFirmwarePort port;
    transfer.handle(geometry(0U, 100U), 0U, port);

    transfer.handle({0xC4U, {}}, 1U, port);

    REQUIRE(!transfer.active());
    REQUIRE_EQ(transfer.frame_data_size(), 0U);
    REQUIRE_EQ(port.events.back(), FirmwareTransferEvent::completed);
}

TEST_CASE(lpc_018_unsolicited_geometry_and_data_work_without_activating_transfer) {
    ControllerFirmwareTransfer transfer;
    FakeFirmwarePort port;

    transfer.handle(geometry(0U, 100U), 0U, port);
    transfer.handle({0xC3U, {0U, 0U, 0U, 1U}}, 1U, port);

    REQUIRE(!transfer.active());
    REQUIRE_EQ(port.sent.back().type, 0xC3U);
}

TEST_CASE(lpc_019_repeated_start_does_not_clear_retained_geometry) {
    ControllerFirmwareTransfer transfer;
    FakeFirmwarePort port;
    transfer.handle(geometry(0U, 100U), 0U, port);

    transfer.handle({0xC1U, {}}, 1U, port);

    REQUIRE_EQ(transfer.frame_data_size(), 100U);
}

TEST_CASE(lpcfw_006_timeout_is_checked_only_after_a_processed_family_frame) {
    ControllerFirmwareTransfer transfer;
    FakeFirmwarePort port;
    transfer.handle({0xC1U, {}}, 0U, port);

    REQUIRE(transfer.active());
    transfer.handle({0xCFU, {}}, 5000U, port);

    REQUIRE(!transfer.active());
    REQUIRE_EQ(port.events.back(), FirmwareTransferEvent::timed_out);
}

TEST_CASE(lpcfw_006_late_valid_geometry_is_processed_before_timeout_check) {
    ControllerFirmwareTransfer transfer;
    FakeFirmwarePort port;
    transfer.handle({0xC1U, {}}, 0U, port);

    transfer.handle(geometry(0U, 100U), 5000U, port);

    REQUIRE(transfer.active());
    REQUIRE_EQ(port.sent.back().type, 0xC2U);
}

TEST_CASE(lpc_016_cancel_ends_transfer_with_cancelled_event) {
    ControllerFirmwareTransfer transfer;
    FakeFirmwarePort port;
    transfer.handle({0xC1U, {}}, 0U, port);

    transfer.handle({0xC5U, {}}, 1U, port);

    REQUIRE(!transfer.active());
    REQUIRE_EQ(port.events.back(), FirmwareTransferEvent::cancelled);
    REQUIRE_EQ(transfer.frame_count(), 0U);
}

TEST_CASE(lpc_013_start_and_geometry_send_failures_publish_errors) {
    ControllerFirmwareTransfer transfer;
    FakeFirmwarePort port;
    port.send_succeeds = false;

    transfer.handle({0xC1U, {}}, 0U, port);
    REQUIRE_EQ(port.events.back(), FirmwareTransferEvent::error);
    REQUIRE(transfer.active());

    port.events.clear();
    transfer.handle(geometry(0U, 100U), 1U, port);
    REQUIRE_EQ(port.events.back(), FirmwareTransferEvent::error);
    REQUIRE_EQ(transfer.frame_data_size(), 100U);
}

TEST_CASE(lpc_013_geometry_rejects_malformed_missing_oversized_and_overflow) {
    ControllerFirmwareTransfer transfer;
    FakeFirmwarePort port;

    transfer.handle({0xC2U, {1U}}, 0U, port);
    REQUIRE_EQ(port.events.back(), FirmwareTransferEvent::error);

    port.size = std::nullopt;
    transfer.handle(geometry(0U, 100U), 0U, port);
    REQUIRE_EQ(port.events.back(), FirmwareTransferEvent::error);

    port.size = 1U;
    transfer.handle(geometry(0U, 1025U), 0U, port);
    REQUIRE_EQ(port.events.back(), FirmwareTransferEvent::error);

    port.size = std::numeric_limits<std::uint64_t>::max();
    transfer.handle(geometry(0U, 1U), 0U, port);
    REQUIRE_EQ(port.events.back(), FirmwareTransferEvent::error);
    REQUIRE_EQ(transfer.frame_count(), 0U);
}

TEST_CASE(lpc_015_data_rejects_malformed_read_failure_and_send_failure) {
    ControllerFirmwareTransfer transfer;
    FakeFirmwarePort port;
    transfer.handle(geometry(0U, 100U), 0U, port);

    transfer.handle({0xC3U, {1U}}, 1U, port);
    REQUIRE_EQ(port.events.back(), FirmwareTransferEvent::error);

    port.read_result = std::nullopt;
    transfer.handle({0xC3U, {0U, 0U, 0U, 1U}}, 1U, port);
    REQUIRE_EQ(port.events.back(), FirmwareTransferEvent::error);

    port.read_result = ByteVector({1U, 2U});
    port.send_succeeds = false;
    transfer.handle({0xC3U, {0U, 0U, 0U, 1U}}, 1U, port);
    REQUIRE_EQ(port.events.back(), FirmwareTransferEvent::error);
}

TEST_CASE(lpcfw_003_data_is_truncated_to_negotiated_frame_size) {
    ControllerFirmwareTransfer transfer;
    FakeFirmwarePort port;
    port.read_result = ByteVector(150U, 0xA5U);
    transfer.handle(geometry(0U, 100U), 0U, port);

    transfer.handle({0xC3U, {0U, 0U, 0U, 1U}}, 1U, port);

    REQUIRE_EQ(port.sent.back().type, 0xC3U);
    REQUIRE_EQ(port.sent.back().payload.size(), 104U);
    REQUIRE_EQ(port.events.back(), FirmwareTransferEvent::progress);
}
