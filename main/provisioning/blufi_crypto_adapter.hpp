/** @file @brief Declares the mbedTLS implementation of portable BLUFI security operations. */
#pragma once

#include "application/provisioning/blufi_security.hpp"

#include "mbedtls/aes.h"
#include "mbedtls/dhm.h"

#include <optional>

namespace firmware::target {

/// Owns one bounded Diffie-Hellman and AES-CFB context for a BLE connection.
class BlufiCryptoAdapter final : public firmware::application::BlufiSecurityPort {
public:
    BlufiCryptoAdapter();
    ~BlufiCryptoAdapter() override;

    std::optional<firmware::core::ByteVector> allocate_parameter_buffer(
        std::size_t size) override;
    firmware::application::BlufiDhResult derive_diffie_hellman(
        firmware::core::BytesView parameters) override;
    std::optional<std::array<std::uint8_t, 16U>> md5(
        firmware::core::BytesView input) override;
    std::optional<firmware::core::ByteVector> aes_cfb128(
        const std::array<std::uint8_t, 16U>& key,
        const std::array<std::uint8_t, 16U>& iv,
        firmware::core::BytesView input, bool encrypt) override;
    void send_negotiation_response(firmware::core::BytesView response) override;
    void report_error(std::uint8_t error) override;
    void report_diagnostic(
        firmware::application::BlufiSecurityDiagnostic diagnostic,
        int first = 0, int second = 0) override;
    int last_crypto_error() const override;

    /// Returns and clears the most recent negotiation response.
    std::optional<firmware::core::ByteVector> take_negotiation_response();

private:
    mbedtls_dhm_context dhm_{};
    mbedtls_aes_context aes_{};
    firmware::core::ByteVector negotiation_response_;
    int last_crypto_error_ = 0;
};

}  // namespace firmware::target
