// Declares the mbedTLS implementation of portable BLUFI security operations.
#pragma once

#include "firmware/application/blufi_security.hpp"

#include "mbedtls/aes.h"
#include "mbedtls/dhm.h"

namespace firmware::target {

// Owns one bounded Diffie-Hellman and AES-CFB context for a BLE connection.
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

private:
    mbedtls_dhm_context dhm_{};
    mbedtls_aes_context aes_{};
};

}  // namespace firmware::target
