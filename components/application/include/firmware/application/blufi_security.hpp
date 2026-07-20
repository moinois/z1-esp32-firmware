// Declares BLUFI negotiation and symmetric security state behind crypto ports.
#pragma once

#include "firmware/core/bytes.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace firmware::application {

// Provides the wire layer only the symmetric operations it requires.
class BlufiCipher {
public:
    // Enables safe destruction through a substituted cipher.
    virtual ~BlufiCipher() = default;

    // Reports whether complete key derivation has succeeded.
    virtual bool ready() const = 0;

    // Encrypts or decrypts using an IV seeded by the sequence number.
    virtual std::optional<core::ByteVector> crypt(std::uint8_t sequence,
                                                  core::BytesView input,
                                                  bool encrypt) = 0;
};

// Identifies deterministic Diffie-Hellman failure stages for error mapping.
enum class BlufiDhFailure {
    none,
    parse,
    public_key,
    shared_secret,
};

// Holds bounded public-key/shared-secret output or its failure stage.
struct BlufiDhResult {
    BlufiDhFailure failure;
    core::ByteVector public_key;
    core::ByteVector shared_secret;
};

// Isolates BLUFI security state from allocation and cryptographic libraries.
class BlufiSecurityPort {
public:
    // Enables safe destruction through a substituted crypto adapter.
    virtual ~BlufiSecurityPort() = default;

    // Allocates a zeroed parameter buffer of the exact announced size.
    virtual std::optional<core::ByteVector> allocate_parameter_buffer(
        std::size_t size) = 0;

    // Parses parameters and generates the local public key and shared secret.
    virtual BlufiDhResult derive_diffie_hellman(
        core::BytesView parameters) = 0;

    // Computes the exact 16-byte MD5 key digest.
    virtual std::optional<std::array<std::uint8_t, 16U>> md5(
        core::BytesView input) = 0;

    // Applies AES-CFB128 with the supplied key, IV, and direction.
    virtual std::optional<core::ByteVector> aes_cfb128(
        const std::array<std::uint8_t, 16U>& key,
        const std::array<std::uint8_t, 16U>& iv, core::BytesView input,
        bool encrypt) = 0;

    // Returns the generated public key as the negotiation response.
    virtual void send_negotiation_response(core::BytesView response) = 0;

    // Sends one exact BLUFI error value.
    virtual void report_error(std::uint8_t error) = 0;
};

// Owns one BLE connection's parameter buffer, AES key, and readiness state.
class BlufiSecurityContext final : public BlufiCipher {
public:
    // Creates an initially unready context using the supplied crypto port.
    explicit BlufiSecurityContext(BlufiSecurityPort& port);

    // Clears all connection-specific security and parameter state.
    void create();

    // Destroys all connection-specific security and parameter state.
    void destroy();

    // Processes one complete negotiation message and reports exact failures.
    void receive_negotiation(core::BytesView message);

    // Reports whether complete key derivation has succeeded for this connection.
    bool ready() const override;

    // Exposes retained parameter capacity for allocation lifecycle verification.
    std::size_t parameter_size() const;

    // Encrypts or decrypts using a fresh IV seeded only by sequence number.
    std::optional<core::ByteVector> crypt(std::uint8_t sequence,
                                         core::BytesView input,
                                         bool encrypt) override;

private:
    // Processes a big-endian parameter length message.
    void receive_parameter_length(core::BytesView message);

    // Processes exact announced parameter bytes and attempts key derivation.
    void receive_parameter_data(core::BytesView message);

    BlufiSecurityPort& port_;
    core::ByteVector parameters_;
    std::array<std::uint8_t, 16U> key_{};
    bool ready_ = false;
};

}  // namespace firmware::application
