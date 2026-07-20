// Implements BLUFI envelopes, sequence consumption, checksum, crypto, and ACKs.
#include "firmware/application/blufi_wire.hpp"

#include "firmware/core/crc.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace firmware::application {
namespace {

constexpr std::size_t frame_header_size = 4U;
constexpr std::size_t checksum_size = 2U;
constexpr std::size_t checksum_input_header_size = 2U;
constexpr std::size_t maximum_data_size = 255U;
constexpr std::uint8_t type_mask = 0x03U;
constexpr unsigned subtype_shift = 2U;
constexpr std::uint8_t encrypted_flag = 0x01U;
constexpr std::uint8_t checksum_flag = 0x02U;
constexpr std::uint8_t outgoing_direction_flag = 0x04U;
constexpr std::uint8_t acknowledgement_flag = 0x08U;
constexpr std::uint8_t non_final_fragment_flag = 0x10U;
constexpr std::uint8_t unsupported_frame_control_mask = 0xE0U;
constexpr std::uint8_t acknowledgement_subtype = 0U;
constexpr std::uint8_t security_mode_subtype = 1U;
constexpr std::uint8_t negotiation_data_subtype = 0U;
constexpr std::uint8_t error_data_subtype = 0x12U;
constexpr std::uint8_t sequence_error = 0U;
constexpr std::uint8_t checksum_error = 1U;
constexpr std::uint8_t decryption_error = 2U;
constexpr std::uint8_t encryption_error = 3U;
constexpr std::uint8_t security_not_initialized_error = 4U;
constexpr std::uint8_t data_format_error = 9U;
constexpr std::uint8_t mode_checksum_ordinary_data = 0x01U;
constexpr std::uint8_t mode_encrypt_ordinary_data = 0x02U;
constexpr std::uint8_t mode_checksum_control_frames = 0x10U;

// Encodes the two-bit type and six-bit subtype into the first header byte.
std::uint8_t type_byte(BlufiFrameType type, std::uint8_t subtype) {
    return static_cast<std::uint8_t>(
        (subtype << subtype_shift) | static_cast<std::uint8_t>(type));
}

// Builds the exact checksum input from sequence, length, and plaintext data.
core::ByteVector checksum_input(std::uint8_t sequence,
                                core::BytesView plaintext) {
    core::ByteVector input;
    input.reserve(checksum_input_header_size + plaintext.size());
    input.push_back(sequence);
    input.push_back(static_cast<std::uint8_t>(plaintext.size()));
    input.insert(input.end(), plaintext.begin(), plaintext.end());
    return input;
}

// Reports whether ordinary-data security explicitly excludes this subtype.
bool security_exempt_data_subtype(std::uint8_t subtype) {
    return subtype == negotiation_data_subtype || subtype == error_data_subtype;
}

}  // namespace

BlufiWireSession::BlufiWireSession(BlufiCipher& cipher, BlufiWirePort& port)
    : cipher_(cipher), port_(port) {}

void BlufiWireSession::reset() {
    incoming_sequence_ = 0U;
    outgoing_sequence_ = 0U;
    checksum_ordinary_data_ = false;
    encrypt_ordinary_data_ = false;
    checksum_control_frames_ = false;
}

bool BlufiWireSession::send(BlufiFrameType type, std::uint8_t subtype,
                            core::BytesView plaintext,
                            bool acknowledgement_requested,
                            bool non_final_fragment) {
    if ((type != BlufiFrameType::control && type != BlufiFrameType::data) ||
        plaintext.size() > maximum_data_size) {
        port_.report_error(data_format_error);
        return false;
    }

    const bool ordinary_data =
        type == BlufiFrameType::data && !security_exempt_data_subtype(subtype);
    const bool add_checksum = ordinary_data ? checksum_ordinary_data_
                                            : type == BlufiFrameType::control &&
                                                  checksum_control_frames_;
    const bool encrypt = ordinary_data && encrypt_ordinary_data_;
    std::uint8_t frame_control = outgoing_direction_flag;
    if (encrypt) {
        frame_control |= encrypted_flag;
    }
    if (add_checksum) {
        frame_control |= checksum_flag;
    }
    if (acknowledgement_requested) {
        frame_control |= acknowledgement_flag;
    }
    if (non_final_fragment) {
        frame_control |= non_final_fragment_flag;
    }

    core::ByteVector wire_data(plaintext.begin(), plaintext.end());
    if (encrypt) {
        if (!cipher_.ready()) {
            port_.report_error(security_not_initialized_error);
            return false;
        }
        auto encrypted = cipher_.crypt(outgoing_sequence_, plaintext, true);
        if (!encrypted.has_value() || encrypted->size() != plaintext.size()) {
            port_.report_error(encryption_error);
            return false;
        }
        wire_data = std::move(*encrypted);
    }

    core::ByteVector frame;
    frame.reserve(frame_header_size + wire_data.size() +
                  (add_checksum ? checksum_size : 0U));
    frame.push_back(type_byte(type, subtype));
    frame.push_back(frame_control);
    frame.push_back(outgoing_sequence_);
    frame.push_back(static_cast<std::uint8_t>(plaintext.size()));
    frame.insert(frame.end(), wire_data.begin(), wire_data.end());
    if (add_checksum) {
        const std::uint16_t checksum =
            core::crc16_blufi(checksum_input(outgoing_sequence_, plaintext));
        frame.push_back(static_cast<std::uint8_t>(checksum & 0xFFU));
        frame.push_back(static_cast<std::uint8_t>(checksum >> 8U));
    }
    port_.send_characteristic(frame);
    ++outgoing_sequence_;
    return true;
}

std::optional<BlufiIncomingFrame> BlufiWireSession::receive(
    core::BytesView frame) {
    if (frame.size() < frame_header_size ||
        (frame[1] & unsupported_frame_control_mask) != 0U) {
        port_.report_error(data_format_error);
        return std::nullopt;
    }
    const bool has_checksum = (frame[1] & checksum_flag) != 0U;
    const std::size_t expected_size =
        frame_header_size + frame[3] + (has_checksum ? checksum_size : 0U);
    if (frame.size() != expected_size) {
        port_.report_error(data_format_error);
        return std::nullopt;
    }
    const std::uint8_t received_sequence = frame[2];
    if (received_sequence != incoming_sequence_) {
        port_.report_error(sequence_error);
        return std::nullopt;
    }
    ++incoming_sequence_;

    core::ByteVector plaintext(frame.begin() + frame_header_size,
                               frame.begin() + frame_header_size + frame[3]);
    if ((frame[1] & encrypted_flag) != 0U) {
        if (!cipher_.ready()) {
            port_.report_error(security_not_initialized_error);
            return std::nullopt;
        }
        auto decrypted = cipher_.crypt(received_sequence, plaintext, false);
        if (!decrypted.has_value() || decrypted->size() != plaintext.size()) {
            port_.report_error(decryption_error);
            return std::nullopt;
        }
        plaintext = std::move(*decrypted);
    }
    if (has_checksum) {
        const std::uint16_t expected_checksum =
            core::crc16_blufi(checksum_input(received_sequence, plaintext));
        const std::size_t checksum_offset = frame_header_size + frame[3];
        const std::uint16_t received_checksum =
            static_cast<std::uint16_t>(frame[checksum_offset]) |
            (static_cast<std::uint16_t>(frame[checksum_offset + 1U]) << 8U);
        if (received_checksum != expected_checksum) {
            port_.report_error(checksum_error);
            return std::nullopt;
        }
    }

    if ((frame[1] & acknowledgement_flag) != 0U) {
        send_acknowledgement(received_sequence);
    }
    const auto type = static_cast<BlufiFrameType>(frame[0] & type_mask);
    const std::uint8_t subtype = frame[0] >> subtype_shift;
    if (type == BlufiFrameType::control &&
        subtype == security_mode_subtype) {
        if (plaintext.empty()) {
            port_.report_error(data_format_error);
            return std::nullopt;
        }
        checksum_ordinary_data_ =
            (plaintext[0] & mode_checksum_ordinary_data) != 0U;
        encrypt_ordinary_data_ =
            (plaintext[0] & mode_encrypt_ordinary_data) != 0U;
        checksum_control_frames_ =
            (plaintext[0] & mode_checksum_control_frames) != 0U;
    }
    return BlufiIncomingFrame{
        type,
        subtype,
        std::move(plaintext),
        (frame[1] & non_final_fragment_flag) != 0U,
    };
}

std::uint8_t BlufiWireSession::next_outgoing_sequence() const {
    return outgoing_sequence_;
}

std::uint8_t BlufiWireSession::next_incoming_sequence() const {
    return incoming_sequence_;
}

void BlufiWireSession::send_acknowledgement(std::uint8_t received_sequence) {
    const core::ByteVector payload{received_sequence};
    static_cast<void>(send(BlufiFrameType::control, acknowledgement_subtype,
                           payload));
}

}  // namespace firmware::application
