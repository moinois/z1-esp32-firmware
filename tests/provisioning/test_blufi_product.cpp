// Verifies BLUFI subtype dispatch and exact product report byte encoding.
#include "test.hpp"

#include "firmware/application/blufi_product.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using firmware::application::BleStationReportState;
using firmware::application::BleWifiListEntry;
using firmware::application::BleWifiStatusReport;
using firmware::application::BlufiFrameType;
using firmware::application::BlufiIncomingFrame;
using firmware::application::BlufiProductDispatcher;
using firmware::application::BlufiProductPort;
using firmware::core::ByteVector;

namespace {

// Records product actions and outgoing data selected by the dispatcher.
class FakeBlufiProductPort final : public BlufiProductPort {
public:
    // Records security negotiation bytes.
    void receive_negotiation(firmware::core::BytesView data) override {
        negotiations.emplace_back(data.begin(), data.end());
    }

    // Records a station-connect request.
    void connect_station() override {
        calls.emplace_back("connect");
    }

    // Records a station-disconnect request.
    void disconnect_station() override {
        calls.emplace_back("disconnect");
    }

    // Records a Wi-Fi status request.
    void request_wifi_status() override {
        calls.emplace_back("status");
    }

    // Records a Wi-Fi list request.
    void request_wifi_list() override {
        calls.emplace_back("list");
    }

    // Records staged SSID bytes.
    void receive_ssid(firmware::core::BytesView data) override {
        ssids.emplace_back(data.begin(), data.end());
    }

    // Records staged password bytes.
    void receive_password(firmware::core::BytesView data) override {
        passwords.emplace_back(data.begin(), data.end());
    }

    // Records one received error value.
    void receive_error(std::uint8_t error) override {
        received_errors.push_back(error);
    }

    // Records custom diagnostic bytes.
    void receive_custom_data(firmware::core::BytesView data) override {
        custom_data.emplace_back(data.begin(), data.end());
    }

    // Records one outgoing product data message.
    void send_data(std::uint8_t subtype,
                   firmware::core::BytesView data) override {
        sent_subtypes.push_back(subtype);
        sent_data.emplace_back(data.begin(), data.end());
    }

    // Records one exact protocol error.
    void report_error(std::uint8_t error) override {
        protocol_errors.push_back(error);
    }

    std::vector<std::string> calls;
    std::vector<ByteVector> negotiations;
    std::vector<ByteVector> ssids;
    std::vector<ByteVector> passwords;
    std::vector<std::uint8_t> received_errors;
    std::vector<ByteVector> custom_data;
    std::vector<std::uint8_t> sent_subtypes;
    std::vector<ByteVector> sent_data;
    std::vector<std::uint8_t> protocol_errors;
};

// Creates one completed incoming BLUFI message.
BlufiIncomingFrame message(BlufiFrameType type, std::uint8_t subtype,
                           ByteVector data = {}) {
    return {type, subtype, std::move(data), false};
}

// Converts readable test text to bytes.
ByteVector bytes(std::string_view text) {
    return {text.begin(), text.end()};
}

}  // namespace

TEST_CASE(bwf_040_control_subtypes_dispatch_product_actions_and_version) {
    FakeBlufiProductPort port;
    BlufiProductDispatcher dispatcher(port);

    dispatcher.dispatch(message(BlufiFrameType::control, 2U, {3U}));
    dispatcher.dispatch(message(BlufiFrameType::control, 3U));
    dispatcher.dispatch(message(BlufiFrameType::control, 4U));
    dispatcher.dispatch(message(BlufiFrameType::control, 5U));
    dispatcher.dispatch(message(BlufiFrameType::control, 7U));
    dispatcher.dispatch(message(BlufiFrameType::control, 9U));

    REQUIRE_EQ(port.calls,
               std::vector<std::string>(
                   {"connect", "disconnect", "status", "list"}));
    REQUIRE_EQ(port.sent_subtypes, std::vector<std::uint8_t>({0x10U}));
    REQUIRE_EQ(port.sent_data,
               std::vector<ByteVector>({{0x01U, 0x03U}}));
}

TEST_CASE(bwf_040_ignored_control_subtypes_have_no_product_effect) {
    FakeBlufiProductPort port;
    BlufiProductDispatcher dispatcher(port);

    dispatcher.dispatch(message(BlufiFrameType::control, 0U, {7U}));
    dispatcher.dispatch(message(BlufiFrameType::control, 1U, {3U}));
    dispatcher.dispatch(message(BlufiFrameType::control, 2U));
    dispatcher.dispatch(message(BlufiFrameType::control, 6U, {1U}));
    dispatcher.dispatch(message(BlufiFrameType::control, 8U));
    dispatcher.dispatch(message(BlufiFrameType::control, 42U));

    REQUIRE(port.calls.empty());
    REQUIRE(port.sent_data.empty());
    REQUIRE(port.protocol_errors.empty());
}

TEST_CASE(bwf_041_data_subtypes_dispatch_only_supported_product_inputs) {
    FakeBlufiProductPort port;
    BlufiProductDispatcher dispatcher(port);

    dispatcher.dispatch(message(BlufiFrameType::data, 0U, {0U, 1U, 2U}));
    dispatcher.dispatch(message(BlufiFrameType::data, 2U, bytes("ssid")));
    dispatcher.dispatch(message(BlufiFrameType::data, 3U, bytes("secret")));
    dispatcher.dispatch(message(BlufiFrameType::data, 0x12U, {7U}));
    dispatcher.dispatch(message(BlufiFrameType::data, 0x13U,
                                bytes("diagnostic")));

    REQUIRE_EQ(port.negotiations,
               std::vector<ByteVector>({{0U, 1U, 2U}}));
    REQUIRE_EQ(port.ssids, std::vector<ByteVector>({bytes("ssid")}));
    REQUIRE_EQ(port.passwords,
               std::vector<ByteVector>({bytes("secret")}));
    REQUIRE_EQ(port.received_errors, std::vector<std::uint8_t>({7U}));
    REQUIRE_EQ(port.custom_data,
               std::vector<ByteVector>({bytes("diagnostic")}));
}

TEST_CASE(bwf_041_unknown_types_and_unsupported_data_have_no_product_effect) {
    FakeBlufiProductPort port;
    BlufiProductDispatcher dispatcher(port);

    dispatcher.dispatch(message(BlufiFrameType::data, 1U, {1U}));
    dispatcher.dispatch(message(BlufiFrameType::data, 4U, {1U}));
    dispatcher.dispatch(message(BlufiFrameType::data, 0x0EU, {1U}));
    dispatcher.dispatch(message(BlufiFrameType::data, 0x20U, {1U}));
    dispatcher.dispatch(message(BlufiFrameType::unknown_two, 2U, {1U}));
    dispatcher.dispatch(message(BlufiFrameType::unknown_three, 3U, {1U}));

    REQUIRE(port.negotiations.empty());
    REQUIRE(port.ssids.empty());
    REQUIRE(port.passwords.empty());
    REQUIRE(port.received_errors.empty());
    REQUIRE(port.custom_data.empty());
    REQUIRE(port.protocol_errors.empty());
}

TEST_CASE(bwf_042_error_payload_requires_exactly_one_byte) {
    FakeBlufiProductPort port;
    BlufiProductDispatcher dispatcher(port);

    dispatcher.dispatch(message(BlufiFrameType::data, 0x12U));
    dispatcher.dispatch(message(BlufiFrameType::data, 0x12U, {1U, 2U}));

    REQUIRE_EQ(port.protocol_errors,
               std::vector<std::uint8_t>({9U, 9U}));
}

TEST_CASE(bwf_043_wifi_status_encoding_has_exact_header_and_tlvs) {
    const BleWifiStatusReport report{
        3U,
        BleStationReportState::connecting,
        {1U, 2U, 3U, 4U, 5U, 6U},
        "ap",
    };

    REQUIRE_EQ(firmware::application::encode_blufi_wifi_status(report),
               ByteVector({3U, 2U, 0U, 1U, 6U, 1U, 2U, 3U, 4U, 5U,
                           6U, 2U, 2U, 'a', 'p'}));
}

TEST_CASE(bwf_044_wifi_list_encoding_preserves_order_and_signed_rssi_byte) {
    const std::vector<BleWifiListEntry> entries{
        {"strong", -30},
        {"x", -128},
    };

    REQUIRE_EQ(firmware::application::encode_blufi_wifi_list(entries),
               ByteVector({7U, 0xE2U, 's', 't', 'r', 'o', 'n', 'g',
                           2U, 0x80U, 'x'}));
}
