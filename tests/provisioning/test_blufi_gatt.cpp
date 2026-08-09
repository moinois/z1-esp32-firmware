// Verifies BLUFI GATT reads, writes, prepared aggregation, and retry policy.
#include "test.hpp"

#include "application/provisioning/blufi_gatt.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using firmware::application::BlufiAttStatus;
using firmware::application::BlufiGattPolicy;
using firmware::application::BlufiGattPort;
using firmware::core::ByteVector;

namespace {

// Substitutes allocation, response, decode, notification, and timing behavior.
class FakeBlufiGattPort final : public BlufiGattPort {
public:
    // Allocates the requested zeroed aggregate unless failure is configured.
    std::optional<ByteVector> allocate_prepared(std::size_t size) override {
        allocation_sizes.push_back(size);
        if (allocation_fails) {
            return std::nullopt;
        }
        return ByteVector(size, 0U);
    }

    // Records an ATT write response in call order.
    void send_write_response(BlufiAttStatus status) override {
        calls.emplace_back("response");
        write_responses.push_back(status);
    }

    // Records a complete value passed to BLUFI frame decoding.
    void decode_frame(firmware::core::BytesView frame) override {
        calls.emplace_back("decode");
        decoded.emplace_back(frame.begin(), frame.end());
    }

    // Attempts one notification and returns its configured outcome.
    bool send_notification(firmware::core::BytesView frame) override {
        notifications.emplace_back(frame.begin(), frame.end());
        const bool result = notification_failures_remaining == 0U;
        if (notification_failures_remaining > 0U) {
            --notification_failures_remaining;
        }
        if (disconnect_on_notification) {
            is_connected = false;
        }
        return result;
    }

    // Reports whether the BLE connection is still present.
    bool connected() const override {
        return is_connected;
    }

    // Records one retry delay.
    void delay_milliseconds(std::uint32_t duration) override {
        delays.push_back(duration);
    }

    bool allocation_fails = false;
    bool is_connected = true;
    bool disconnect_on_notification = false;
    std::size_t notification_failures_remaining = 0U;
    std::vector<std::string> calls;
    std::vector<std::size_t> allocation_sizes;
    std::vector<BlufiAttStatus> write_responses;
    std::vector<ByteVector> decoded;
    std::vector<ByteVector> notifications;
    std::vector<std::uint32_t> delays;
};

}  // namespace

TEST_CASE(bwf_003_outgoing_characteristic_and_descriptor_read_as_single_zero) {
    FakeBlufiGattPort port;
    BlufiGattPolicy gatt(port);

    REQUIRE_EQ(gatt.read_outgoing_characteristic(), ByteVector({0U}));
    REQUIRE_EQ(gatt.read_client_configuration(), ByteVector({0U}));
}

TEST_CASE(bwf_003_and_007_normal_writes_respond_before_control_decode) {
    FakeBlufiGattPort port;
    BlufiGattPolicy gatt(port);

    gatt.write_control(ByteVector({1U, 2U}), true);
    gatt.write_client_configuration(ByteVector({1U, 0U}), true);

    REQUIRE_EQ(port.calls,
               std::vector<std::string>({"response", "decode", "response"}));
    REQUIRE_EQ(port.write_responses,
               std::vector<BlufiAttStatus>({BlufiAttStatus::success,
                                            BlufiAttStatus::success}));
    REQUIRE_EQ(port.decoded, std::vector<ByteVector>({{1U, 2U}}));
}

TEST_CASE(bwf_007_write_without_response_decodes_without_att_response) {
    FakeBlufiGattPort port;
    BlufiGattPolicy gatt(port);

    gatt.write_control(ByteVector({3U}), false);
    gatt.write_client_configuration(ByteVector({1U}), false);

    REQUIRE(port.write_responses.empty());
    REQUIRE_EQ(port.decoded, std::vector<ByteVector>({{3U}}));
}

TEST_CASE(bwf_005_to_006_prepared_writes_share_buffer_echo_and_execute) {
    FakeBlufiGattPort port;
    BlufiGattPolicy gatt(port);

    const auto first =
        gatt.prepare_write(0x10U, 0U, ByteVector({'a', 'b'}));
    const auto second = gatt.prepare_write(0x20U, 2U, ByteVector({'c'}));
    const BlufiAttStatus execute_status = gatt.finish_prepared(true);

    REQUIRE_EQ(first.status, BlufiAttStatus::success);
    REQUIRE_EQ(first.handle, 0x10U);
    REQUIRE_EQ(first.offset, 0U);
    REQUIRE_EQ(first.value, ByteVector({'a', 'b'}));
    REQUIRE_EQ(second.status, BlufiAttStatus::success);
    REQUIRE_EQ(second.handle, 0x20U);
    REQUIRE_EQ(second.offset, 2U);
    REQUIRE_EQ(second.value, ByteVector({'c'}));
    REQUIRE_EQ(port.allocation_sizes, std::vector<std::size_t>({1024U}));
    REQUIRE_EQ(execute_status, BlufiAttStatus::success);
    REQUIRE_EQ(port.decoded, std::vector<ByteVector>({{'a', 'b', 'c'}}));
}

TEST_CASE(bwf_006_cancel_discards_aggregate_and_always_returns_success) {
    FakeBlufiGattPort port;
    BlufiGattPolicy gatt(port);

    REQUIRE_EQ(gatt.prepare_write(1U, 0U, ByteVector({'a'})).status,
               BlufiAttStatus::success);
    REQUIRE_EQ(gatt.finish_prepared(false), BlufiAttStatus::success);
    REQUIRE_EQ(gatt.finish_prepared(true), BlufiAttStatus::success);

    REQUIRE(port.decoded.empty());
}

TEST_CASE(bwf_004_connection_reset_discards_prepared_aggregate) {
    FakeBlufiGattPort port;
    BlufiGattPolicy gatt(port);

    REQUIRE_EQ(gatt.prepare_write(1U, 0U, ByteVector({'a'})).status,
               BlufiAttStatus::success);
    gatt.reset();
    REQUIRE_EQ(gatt.finish_prepared(true), BlufiAttStatus::success);

    REQUIRE(port.decoded.empty());
}

TEST_CASE(bwf_005_prepared_rejections_have_exact_status_and_discard_state) {
    FakeBlufiGattPort port;
    BlufiGattPolicy gatt(port);

    REQUIRE_EQ(gatt.prepare_write(1U, 1U, ByteVector({'a'})).status,
               BlufiAttStatus::invalid_offset);
    REQUIRE_EQ(gatt.prepare_write(1U, 1025U, ByteVector{}).status,
               BlufiAttStatus::invalid_offset);
    REQUIRE_EQ(gatt.prepare_write(1U, 0U, ByteVector(1025U, 0U)).status,
               BlufiAttStatus::invalid_length);

    port.allocation_fails = true;
    REQUIRE_EQ(gatt.prepare_write(1U, 0U, ByteVector({'a'})).status,
               BlufiAttStatus::allocation_failure);
    REQUIRE_EQ(gatt.finish_prepared(true), BlufiAttStatus::success);
    REQUIRE(port.decoded.empty());
}

TEST_CASE(bwf_005_rejected_later_write_discards_complete_prepared_aggregate) {
    FakeBlufiGattPort port;
    BlufiGattPolicy gatt(port);

    REQUIRE_EQ(gatt.prepare_write(1U, 0U, ByteVector({'a'})).status,
               BlufiAttStatus::success);
    REQUIRE_EQ(gatt.prepare_write(2U, 1024U, ByteVector({'b'})).status,
               BlufiAttStatus::invalid_length);
    REQUIRE_EQ(gatt.finish_prepared(true), BlufiAttStatus::success);

    REQUIRE(port.decoded.empty());
}

TEST_CASE(bwf_045_notification_retries_every_ten_ms_until_success) {
    FakeBlufiGattPort port;
    port.notification_failures_remaining = 2U;
    BlufiGattPolicy gatt(port);

    REQUIRE(gatt.notify(ByteVector({1U, 2U})));

    REQUIRE_EQ(port.notifications,
               std::vector<ByteVector>({{1U, 2U}, {1U, 2U}, {1U, 2U}}));
    REQUIRE_EQ(port.delays, std::vector<std::uint32_t>({10U, 10U}));
}

TEST_CASE(bwf_045_notification_stops_without_delay_when_connection_disappears) {
    FakeBlufiGattPort port;
    port.notification_failures_remaining = 1U;
    port.disconnect_on_notification = true;
    BlufiGattPolicy gatt(port);

    REQUIRE(!gatt.notify(ByteVector({7U})));

    REQUIRE_EQ(port.notifications, std::vector<ByteVector>({{7U}}));
    REQUIRE(port.delays.empty());
}
