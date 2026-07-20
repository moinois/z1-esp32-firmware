// Declares one BLUFI connection's frame envelope and sequence state.
#pragma once

#include "firmware/application/blufi_security.hpp"
#include "firmware/core/bytes.hpp"

#include <cstdint>
#include <optional>

namespace firmware::application {

// Represents the low two BLUFI type bits, including ignorable unknown values.
enum class BlufiFrameType : std::uint8_t {
    control = 0U,
    data = 1U,
    unknown_two = 2U,
    unknown_three = 3U,
};

// Holds one validated, decrypted incoming frame for reassembly or dispatch.
struct BlufiIncomingFrame {
    BlufiFrameType type;
    std::uint8_t subtype;
    core::ByteVector data;
    bool non_final_fragment;
};

// Isolates frame policy from characteristic notifications and error reporting.
class BlufiWirePort {
public:
    // Enables safe destruction through a substituted wire adapter.
    virtual ~BlufiWirePort() = default;

    // Sends one complete characteristic value as a notification.
    virtual void send_characteristic(core::BytesView frame) = 0;

    // Sends one exact BLUFI protocol error value.
    virtual void report_error(std::uint8_t error) = 0;
};

// Owns incoming/outgoing sequence numbers and selected security-mode flags.
class BlufiWireSession {
public:
    // Binds the wire session to symmetric security and notification adapters.
    BlufiWireSession(BlufiCipher& cipher, BlufiWirePort& port);

    // Resets both sequences and selected security mode for a new connection.
    void reset();

    // Encodes and sends one frame when its payload fits the one-byte length.
    bool send(BlufiFrameType type, std::uint8_t subtype,
              core::BytesView plaintext, bool acknowledgement_requested = false,
              bool non_final_fragment = false);

    // Validates, consumes, decrypts, acknowledges, and returns one frame.
    std::optional<BlufiIncomingFrame> receive(core::BytesView frame);

    // Exposes the next outgoing value for wrap and lifecycle verification.
    std::uint8_t next_outgoing_sequence() const;

    // Exposes the next expected incoming value for failure-order verification.
    std::uint8_t next_incoming_sequence() const;

private:
    // Sends acknowledgement control subtype zero with the received sequence.
    void send_acknowledgement(std::uint8_t received_sequence);

    BlufiCipher& cipher_;
    BlufiWirePort& port_;
    std::uint8_t incoming_sequence_ = 0U;
    std::uint8_t outgoing_sequence_ = 0U;
    bool checksum_ordinary_data_ = false;
    bool encrypt_ordinary_data_ = false;
    bool checksum_control_frames_ = false;
};

}  // namespace firmware::application
