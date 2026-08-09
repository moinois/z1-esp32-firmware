// Verifies complete BLUFI wire, fragment, security, and product composition.
#include "test.hpp"

#include "application/provisioning/blufi_session.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using firmware::application::BlufiCipher;
using firmware::application::BlufiConnectionSession;
using firmware::application::BlufiNegotiationHandler;
using firmware::application::BlufiProductActions;
using firmware::application::BlufiSessionTransport;
using firmware::core::ByteVector;

namespace {

// Applies deterministic reversible crypto and exposes mutable readiness.
class FakeSessionCipher final : public BlufiCipher {
public:
    // Reports whether negotiation has made symmetric crypto ready.
    bool ready() const override {
        return is_ready;
    }

    // XORs all bytes for deterministic encryption and decryption.
    std::optional<ByteVector> crypt(std::uint8_t sequence,
                                    firmware::core::BytesView input,
                                    bool encrypt) override {
        sequences.push_back(sequence);
        directions.push_back(encrypt);
        ByteVector output(input.begin(), input.end());
        for (std::uint8_t& byte : output) {
            byte ^= 0x80U;
        }
        return output;
    }

    bool is_ready = false;
    std::vector<std::uint8_t> sequences;
    std::vector<bool> directions;
};

// Records negotiation and marks the fake cipher ready on kind-one input.
class FakeNegotiationHandler final : public BlufiNegotiationHandler {
public:
    // Binds readiness changes to the supplied cipher.
    explicit FakeNegotiationHandler(FakeSessionCipher& bound_cipher)
        : cipher(bound_cipher) {}

    // Records a message and completes security for kind one.
    void receive_negotiation(firmware::core::BytesView data) override {
        messages.emplace_back(data.begin(), data.end());
        if (data.size() > 0U && data[0] == 1U) {
            cipher.is_ready = true;
        }
    }

    FakeSessionCipher& cipher;
    std::vector<ByteVector> messages;
};

// Records product actions selected after complete frame reassembly.
class FakeProductActions final : public BlufiProductActions {
public:
    // Records completed security negotiation.
    void security_negotiated() override {
        calls.emplace_back("secure");
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

    // Records received station SSID bytes.
    void receive_ssid(firmware::core::BytesView data) override {
        calls.emplace_back("ssid");
        ssids.emplace_back(data.begin(), data.end());
    }

    // Records received station password bytes.
    void receive_password(firmware::core::BytesView data) override {
        calls.emplace_back("password");
        passwords.emplace_back(data.begin(), data.end());
    }

    // Records one received protocol error.
    void receive_error(std::uint8_t error) override {
        received_errors.push_back(error);
    }

    // Records custom diagnostic data.
    void receive_custom_data(firmware::core::BytesView data) override {
        custom_data.emplace_back(data.begin(), data.end());
    }

    std::vector<std::string> calls;
    std::vector<ByteVector> ssids;
    std::vector<ByteVector> passwords;
    std::vector<std::uint8_t> received_errors;
    std::vector<ByteVector> custom_data;
};

// Records characteristic notifications and provides exact message buffers.
class FakeSessionTransport final : public BlufiSessionTransport {
public:
    // Records one complete outgoing characteristic value.
    void send_characteristic(firmware::core::BytesView frame) override {
        sent.emplace_back(frame.begin(), frame.end());
    }

    // Allocates an exact zeroed fragmented-message buffer.
    std::optional<ByteVector> allocate_message(std::size_t size) override {
        return ByteVector(size, 0U);
    }

    std::vector<ByteVector> sent;
};

// Holds all replaceable dependencies for one composed-session test.
struct SessionFixture {
    FakeSessionCipher cipher;
    FakeNegotiationHandler negotiation{cipher};
    FakeProductActions actions;
    FakeSessionTransport transport;
    BlufiConnectionSession session{cipher, negotiation, actions, transport};
};

// Builds an incoming data frame with the requested subtype and flags.
ByteVector incoming_frame(std::uint8_t subtype, std::uint8_t flags,
                          std::uint8_t sequence, ByteVector data) {
    ByteVector frame{
        static_cast<std::uint8_t>((subtype << 2U) | 1U),
        flags,
        sequence,
        static_cast<std::uint8_t>(data.size()),
    };
    frame.insert(frame.end(), data.begin(), data.end());
    return frame;
}

// Builds an incoming control frame with no product payload.
ByteVector incoming_control(std::uint8_t subtype, std::uint8_t sequence) {
    return ByteVector{
        static_cast<std::uint8_t>(subtype << 2U), 0U, sequence, 0U};
}

}  // namespace

TEST_CASE(blufi_session_dispatches_plain_data_from_characteristic_input) {
    SessionFixture fixture;

    fixture.session.receive_characteristic(
        incoming_frame(2U, 0U, 0U, {'a', 'p'}));

    REQUIRE_EQ(fixture.actions.ssids,
               std::vector<ByteVector>({{'a', 'p'}}));
    REQUIRE(fixture.transport.sent.empty());
}

TEST_CASE(blufi_session_acknowledges_then_reassembles_before_dispatch) {
    SessionFixture fixture;

    fixture.session.receive_characteristic(
        incoming_frame(2U, 0x18U, 0U, {3U, 0U, 'a', 'b'}));
    REQUIRE_EQ(fixture.transport.sent,
               std::vector<ByteVector>({{0U, 0x04U, 0U, 1U, 0U}}));
    REQUIRE(fixture.actions.ssids.empty());
    fixture.session.receive_characteristic(
        incoming_frame(2U, 0U, 1U, {'c'}));

    REQUIRE_EQ(fixture.actions.ssids,
               std::vector<ByteVector>({{'a', 'b', 'c'}}));
}

TEST_CASE(blufi_session_notifies_product_once_when_negotiation_becomes_ready) {
    SessionFixture fixture;

    fixture.session.receive_characteristic(
        incoming_frame(0U, 0U, 0U, {1U, 0U, 0U}));
    fixture.session.receive_characteristic(
        incoming_frame(0U, 0U, 1U, {1U, 0U, 0U}));

    REQUIRE_EQ(fixture.negotiation.messages.size(), 2U);
    REQUIRE_EQ(fixture.actions.calls, std::vector<std::string>({"secure"}));
}

TEST_CASE(blufi_session_reports_wire_errors_as_unsecured_error_data) {
    SessionFixture fixture;

    fixture.session.receive_characteristic(ByteVector({1U, 2U, 3U}));

    REQUIRE_EQ(fixture.transport.sent,
               std::vector<ByteVector>({{0x49U, 0x04U, 0U, 1U, 9U}}));
}

TEST_CASE(blufi_session_outgoing_logical_data_uses_fragment_wire_sequences) {
    SessionFixture fixture;
    const ByteVector message(13U, 'x');

    REQUIRE(fixture.session.send_product_data(0x13U, message));

    REQUIRE_EQ(fixture.transport.sent.size(), 2U);
    REQUIRE_EQ(fixture.transport.sent[0][0], 0x4DU);
    REQUIRE_EQ(fixture.transport.sent[0][1], 0x14U);
    REQUIRE_EQ(fixture.transport.sent[0][2], 0U);
    REQUIRE_EQ(fixture.transport.sent[0][3], 14U);
    REQUIRE_EQ(fixture.transport.sent[0][4], 13U);
    REQUIRE_EQ(fixture.transport.sent[0][5], 0U);
    REQUIRE_EQ(fixture.transport.sent[1],
               ByteVector({0x4DU, 0x04U, 1U, 1U, 'x'}));
}

TEST_CASE(blufi_session_reset_clears_sequences_fragments_and_security_notice) {
    SessionFixture fixture;
    fixture.session.receive_characteristic(
        incoming_frame(2U, 0x10U, 0U, {2U, 0U, 'a'}));
    fixture.session.reset();
    fixture.session.receive_characteristic(
        incoming_frame(2U, 0U, 0U, {'b'}));

    REQUIRE_EQ(fixture.actions.ssids,
               std::vector<ByteVector>({{'b'}}));
    REQUIRE(fixture.session.send_product_data(0x13U, ByteVector({'x'})));
    REQUIRE_EQ(fixture.transport.sent.back()[2], 0U);
}

TEST_CASE(blufi_session_forwards_control_and_data_product_actions) {
    SessionFixture fixture;

    fixture.session.receive_characteristic(incoming_control(3U, 0U));
    fixture.session.receive_characteristic(incoming_control(4U, 1U));
    fixture.session.receive_characteristic(incoming_control(5U, 2U));
    fixture.session.receive_characteristic(incoming_control(9U, 3U));
    fixture.session.receive_characteristic(
        incoming_frame(3U, 0U, 4U, {'p', 'w'}));
    fixture.session.receive_characteristic(
        incoming_frame(0x12U, 0U, 5U, {7U}));
    fixture.session.receive_characteristic(
        incoming_frame(0x13U, 0U, 6U, {'d'}));

    REQUIRE_EQ(fixture.actions.calls,
               std::vector<std::string>({"connect", "disconnect", "status",
                                         "list", "password"}));
    REQUIRE_EQ(fixture.actions.passwords,
               std::vector<ByteVector>({{'p', 'w'}}));
    REQUIRE_EQ(fixture.actions.received_errors,
               std::vector<std::uint8_t>({7U}));
    REQUIRE_EQ(fixture.actions.custom_data,
               std::vector<ByteVector>({{'d'}}));
}

TEST_CASE(blufi_session_applies_mtu_and_sends_product_generated_data) {
    SessionFixture fixture;
    fixture.session.set_att_mtu(40U);

    REQUIRE(fixture.session.send_product_data(0x13U, ByteVector(13U, 'x')));
    REQUIRE_EQ(fixture.transport.sent.size(), 1U);
    fixture.session.receive_characteristic(incoming_control(7U, 0U));

    REQUIRE_EQ(fixture.transport.sent.size(), 2U);
    REQUIRE_EQ(fixture.transport.sent.back(),
               ByteVector({0x41U, 0x04U, 1U, 2U, 0x01U, 0x03U}));
}

TEST_CASE(blufi_session_reports_explicit_and_product_format_errors) {
    SessionFixture fixture;

    fixture.session.report_protocol_error(4U);
    fixture.session.receive_characteristic(
        incoming_frame(0x12U, 0U, 0U, {1U, 2U}));

    REQUIRE_EQ(fixture.transport.sent,
               std::vector<ByteVector>({
                   {0x49U, 0x04U, 0U, 1U, 4U},
                   {0x49U, 0x04U, 1U, 1U, 9U},
               }));
}
