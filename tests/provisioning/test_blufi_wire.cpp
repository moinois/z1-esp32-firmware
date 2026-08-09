// Verifies BLUFI envelopes, sequences, security flags, checksums, and ACKs.
#include "test.hpp"

#include "application/provisioning/blufi_wire.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

using firmware::application::BlufiCipher;
using firmware::application::BlufiFrameType;
using firmware::application::BlufiWirePort;
using firmware::application::BlufiWireSession;
using firmware::core::ByteVector;

namespace {

// Applies a reversible test transform and records sequence-driven crypto calls.
class FakeBlufiCipher final : public BlufiCipher {
public:
    // Reports whether a derived session key is available.
    bool ready() const override {
        return is_ready;
    }

    // XORs bytes for deterministic encryption/decryption or returns failure.
    std::optional<ByteVector> crypt(std::uint8_t sequence,
                                    firmware::core::BytesView input,
                                    bool encrypt) override {
        sequences.push_back(sequence);
        directions.push_back(encrypt);
        if (crypto_fails) {
            return std::nullopt;
        }
        ByteVector output(input.begin(), input.end());
        for (std::uint8_t& byte : output) {
            byte ^= 0x80U;
        }
        return output;
    }

    bool is_ready = true;
    bool crypto_fails = false;
    std::vector<std::uint8_t> sequences;
    std::vector<bool> directions;
};

// Records transmitted characteristic values and protocol errors.
class FakeBlufiWirePort final : public BlufiWirePort {
public:
    // Records one complete outgoing characteristic value.
    void send_characteristic(firmware::core::BytesView frame) override {
        sent.emplace_back(frame.begin(), frame.end());
    }

    // Records one exact protocol error value.
    void report_error(std::uint8_t error) override {
        errors.push_back(error);
    }

    std::vector<ByteVector> sent;
    std::vector<std::uint8_t> errors;
};

// Sends an incoming security-mode control frame at the next sequence value.
void select_mode(BlufiWireSession& session, std::uint8_t mode,
                 std::uint8_t sequence = 0U) {
    const ByteVector frame{0x04U, 0U, sequence, 1U, mode};
    REQUIRE(session.receive(frame).has_value());
}

}  // namespace

TEST_CASE(bwf_010_to_013_plain_outgoing_frame_has_exact_header_and_sequence) {
    FakeBlufiCipher cipher;
    FakeBlufiWirePort port;
    BlufiWireSession session(cipher, port);

    REQUIRE(session.send(BlufiFrameType::data, 2U, ByteVector({0xAAU})));
    REQUIRE(session.send(BlufiFrameType::control, 3U, ByteVector{}));

    REQUIRE_EQ(port.sent,
               std::vector<ByteVector>({{0x09U, 0x04U, 0U, 1U, 0xAAU},
                                        {0x0CU, 0x04U, 1U, 0U}}));
    REQUIRE_EQ(session.next_outgoing_sequence(), 2U);
    session.reset();
    REQUIRE_EQ(session.next_outgoing_sequence(), 0U);
    REQUIRE_EQ(session.next_incoming_sequence(), 0U);
}

TEST_CASE(bwf_013_outgoing_sequence_wraps_modulo_256) {
    FakeBlufiCipher cipher;
    FakeBlufiWirePort port;
    BlufiWireSession session(cipher, port);

    for (std::size_t count = 0U; count < 256U; ++count) {
        REQUIRE(session.send(BlufiFrameType::control, 0U, ByteVector{}));
    }

    REQUIRE_EQ(port.sent.front()[2], 0U);
    REQUIRE_EQ(port.sent.back()[2], 255U);
    REQUIRE_EQ(session.next_outgoing_sequence(), 0U);
}

TEST_CASE(bwf_020_to_023_mode_encrypts_data_and_keeps_plaintext_crc_little_endian) {
    FakeBlufiCipher cipher;
    FakeBlufiWirePort port;
    BlufiWireSession session(cipher, port);
    select_mode(session, 0x33U);

    REQUIRE(session.send(BlufiFrameType::data, 2U,
                         ByteVector({'A', 'B'})));

    REQUIRE_EQ(port.sent[0],
               ByteVector({0x09U, 0x07U, 0U, 2U, 0xC1U, 0xC2U,
                           0x24U, 0x43U}));
    REQUIRE_EQ(cipher.sequences, std::vector<std::uint8_t>({0U}));
    REQUIRE_EQ(cipher.directions, std::vector<bool>({true}));
}

TEST_CASE(bwf_023_negotiation_and_error_data_ignore_ordinary_security_mode) {
    FakeBlufiCipher cipher;
    FakeBlufiWirePort port;
    BlufiWireSession session(cipher, port);
    select_mode(session, 0x03U);

    REQUIRE(session.send(BlufiFrameType::data, 0U, ByteVector({1U})));
    REQUIRE(session.send(BlufiFrameType::data, 0x12U, ByteVector({2U})));

    REQUIRE_EQ(port.sent[0][1], 0x04U);
    REQUIRE_EQ(port.sent[1][1], 0x04U);
    REQUIRE(cipher.sequences.empty());
}

TEST_CASE(bwf_023_control_mode_enables_checksum_but_bit_five_never_encrypts) {
    FakeBlufiCipher cipher;
    FakeBlufiWirePort port;
    BlufiWireSession session(cipher, port);
    select_mode(session, 0x30U);

    REQUIRE(session.send(BlufiFrameType::control, 3U, ByteVector({7U})));

    REQUIRE_EQ(port.sent[0],
               ByteVector({0x0CU, 0x06U, 0U, 1U, 7U, 0xB5U, 0x70U}));
    REQUIRE(cipher.sequences.empty());
}

TEST_CASE(bwf_010_incoming_envelope_rejects_short_wrong_length_and_trailing_bytes) {
    FakeBlufiCipher cipher;
    FakeBlufiWirePort port;
    BlufiWireSession session(cipher, port);

    REQUIRE(!session.receive(ByteVector({1U, 2U, 3U})).has_value());
    REQUIRE(!session.receive(ByteVector({9U, 0U, 0U, 2U, 7U})).has_value());
    REQUIRE(!session.receive(ByteVector({9U, 0U, 0U, 1U, 7U, 8U}))
                 .has_value());

    REQUIRE_EQ(port.errors, std::vector<std::uint8_t>({9U, 9U, 9U}));
    REQUIRE_EQ(session.next_incoming_sequence(), 0U);
}

TEST_CASE(bwf_012_sequence_mismatch_does_not_advance_but_match_advances_early) {
    FakeBlufiCipher cipher;
    FakeBlufiWirePort port;
    BlufiWireSession session(cipher, port);

    REQUIRE(!session.receive(ByteVector({9U, 0U, 1U, 0U})).has_value());
    REQUIRE_EQ(session.next_incoming_sequence(), 0U);
    cipher.is_ready = false;
    REQUIRE(!session.receive(ByteVector({9U, 1U, 0U, 1U, 7U}))
                 .has_value());
    REQUIRE_EQ(session.next_incoming_sequence(), 1U);
    REQUIRE(session.receive(ByteVector({9U, 0U, 1U, 1U, 8U})).has_value());

    REQUIRE_EQ(port.errors, std::vector<std::uint8_t>({0U, 4U}));
}

TEST_CASE(bwf_020_to_022_incoming_decrypts_before_plaintext_checksum_validation) {
    FakeBlufiCipher cipher;
    FakeBlufiWirePort port;
    BlufiWireSession session(cipher, port);

    const auto decoded = session.receive(
        ByteVector({0x09U, 0x03U, 0U, 1U, 0xC1U, 0xB7U, 0x58U}));

    REQUIRE(decoded.has_value());
    REQUIRE_EQ(decoded->type, BlufiFrameType::data);
    REQUIRE_EQ(decoded->subtype, 2U);
    REQUIRE_EQ(decoded->data, ByteVector({'A'}));
    REQUIRE_EQ(cipher.directions, std::vector<bool>({false}));
}

TEST_CASE(bwf_022_crypto_and_checksum_failures_have_exact_errors) {
    FakeBlufiCipher cipher;
    FakeBlufiWirePort port;
    BlufiWireSession session(cipher, port);
    cipher.crypto_fails = true;

    REQUIRE(!session.receive(ByteVector({9U, 1U, 0U, 1U, 0xC1U}))
                 .has_value());
    cipher.crypto_fails = false;
    REQUIRE(!session.receive(
                 ByteVector({9U, 2U, 1U, 1U, 'A', 0U, 0U}))
                 .has_value());

    REQUIRE_EQ(port.errors, std::vector<std::uint8_t>({2U, 1U}));
    REQUIRE_EQ(session.next_incoming_sequence(), 2U);
}

TEST_CASE(bwf_033_acknowledgement_is_sent_after_validation_with_received_sequence) {
    FakeBlufiCipher cipher;
    FakeBlufiWirePort port;
    BlufiWireSession session(cipher, port);

    const auto decoded =
        session.receive(ByteVector({9U, 0x08U, 0U, 1U, 7U}));

    REQUIRE(decoded.has_value());
    REQUIRE_EQ(port.sent,
               std::vector<ByteVector>({{0U, 0x04U, 0U, 1U, 0U}}));
}

TEST_CASE(bwf_023_security_mode_is_applied_from_first_control_data_byte) {
    FakeBlufiCipher cipher;
    FakeBlufiWirePort port;
    BlufiWireSession session(cipher, port);

    select_mode(session, 0x03U);
    REQUIRE(session.send(BlufiFrameType::data, 1U, ByteVector({1U})));
    REQUIRE_EQ(port.sent[0][1], 0x07U);

    const ByteVector empty_mode{0x04U, 0U, 1U, 0U};
    REQUIRE(!session.receive(empty_mode).has_value());
    REQUIRE_EQ(port.errors, std::vector<std::uint8_t>({9U}));
}
