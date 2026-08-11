/** @file @brief Implements BLUFI DH, MD5, and AES-CFB128 operations with ESP-IDF mbedTLS. */
#include "blufi_crypto_adapter.hpp"

#include "esp_blufi_api.h"
#include "esp_log.h"
#include "esp_random.h"
#include "mbedtls/md5.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <utility>

namespace firmware::target {
namespace {

constexpr std::size_t maximum_parameter_size = 128U;
constexpr std::size_t maximum_key_size = 128U;

int random_bytes(void*, unsigned char* output, std::size_t size) {
    esp_fill_random(output, size);
    return 0;
}

}  // namespace

BlufiCryptoAdapter::BlufiCryptoAdapter() {
    mbedtls_dhm_init(&dhm_);
    mbedtls_aes_init(&aes_);
}

BlufiCryptoAdapter::~BlufiCryptoAdapter() {
    mbedtls_dhm_free(&dhm_);
    mbedtls_aes_free(&aes_);
}

std::optional<firmware::core::ByteVector>
BlufiCryptoAdapter::allocate_parameter_buffer(std::size_t size) {
    if (size == 0U || size > maximum_parameter_size) {
        return std::nullopt;
    }
    return firmware::core::ByteVector(size, 0U);
}

firmware::application::BlufiDhResult
BlufiCryptoAdapter::derive_diffie_hellman(firmware::core::BytesView parameters) {
    if (parameters.size() == 0U || parameters.size() > maximum_parameter_size) {
        return {firmware::application::BlufiDhFailure::parse, {}, {}};
    }
    auto* begin = const_cast<unsigned char*>(parameters.data());
    auto* end = begin + parameters.size();
    const int parse_result = mbedtls_dhm_read_params(&dhm_, &begin, end);
    if (parse_result != 0) {
        return {firmware::application::BlufiDhFailure::parse, {}, {}, parse_result};
    }
    const std::size_t key_size = static_cast<std::size_t>(mbedtls_dhm_get_len(&dhm_));
    if (key_size == 0U || key_size > maximum_key_size) {
        return {firmware::application::BlufiDhFailure::public_key, {}, {}};
    }
    firmware::core::ByteVector public_key(key_size);
    const int public_result = mbedtls_dhm_make_public(
        &dhm_, static_cast<int>(key_size), public_key.data(), public_key.size(),
        random_bytes, nullptr);
    if (public_result != 0) {
        return {firmware::application::BlufiDhFailure::public_key, {}, {},
                public_result};
    }
    firmware::core::ByteVector shared_secret(maximum_key_size);
    std::size_t shared_size = 0U;
    const int secret_result = mbedtls_dhm_calc_secret(
        &dhm_, shared_secret.data(), shared_secret.size(), &shared_size,
        random_bytes, nullptr);
    if (secret_result != 0) {
        return {firmware::application::BlufiDhFailure::shared_secret, {}, {},
                secret_result};
    }
    shared_secret.resize(shared_size);
    return {firmware::application::BlufiDhFailure::none,
            std::move(public_key), std::move(shared_secret)};
}

std::optional<std::array<std::uint8_t, 16U>>
BlufiCryptoAdapter::md5(firmware::core::BytesView input) {
    std::array<std::uint8_t, 16U> digest{};
    mbedtls_md5_context context;
    mbedtls_md5_init(&context);
    const int result = mbedtls_md5_starts(&context) |
                       mbedtls_md5_update(&context, input.data(), input.size()) |
                       mbedtls_md5_finish(&context, digest.data());
    mbedtls_md5_free(&context);
    last_crypto_error_ = result;
    if (result != 0) {
        return std::nullopt;
    }
    return digest;
}

std::optional<firmware::core::ByteVector> BlufiCryptoAdapter::aes_cfb128(
    const std::array<std::uint8_t, 16U>& key,
    const std::array<std::uint8_t, 16U>& iv,
    firmware::core::BytesView input, bool encrypt) {
    if (mbedtls_aes_setkey_enc(&aes_, key.data(), key.size() * 8U) != 0) {
        return std::nullopt;
    }
    firmware::core::ByteVector output(input.begin(), input.end());
    std::array<std::uint8_t, 16U> working_iv = iv;
    std::size_t offset = 0U;
    const int mode = encrypt ? MBEDTLS_AES_ENCRYPT : MBEDTLS_AES_DECRYPT;
    if (mbedtls_aes_crypt_cfb128(&aes_, mode, output.size(), &offset,
                                 working_iv.data(), output.data(), output.data()) != 0) {
        return std::nullopt;
    }
    return output;
}

void BlufiCryptoAdapter::send_negotiation_response(
    firmware::core::BytesView response) {
    negotiation_response_.assign(response.begin(), response.end());
}

void BlufiCryptoAdapter::report_error(std::uint8_t error) {
    static_cast<void>(esp_blufi_send_error_info(
        static_cast<esp_blufi_error_state_t>(error)));
}

void BlufiCryptoAdapter::report_diagnostic(
    firmware::application::BlufiSecurityDiagnostic diagnostic,
    int first, int second) {
    using firmware::application::BlufiSecurityDiagnostic;
    constexpr char tag[] = "BLUFI_SEC";
    switch (diagnostic) {
        case BlufiSecurityDiagnostic::initialized: ESP_LOGI(tag, "DH security init done"); break;
        case BlufiSecurityDiagnostic::deinitialized: ESP_LOGI(tag, "DH security deinit done"); break;
        case BlufiSecurityDiagnostic::invalid_format: ESP_LOGE(tag, "Invalid negotiate data format"); break;
        case BlufiSecurityDiagnostic::uninitialized: ESP_LOGE(tag, "Security not initialized"); break;
        case BlufiSecurityDiagnostic::parameter_allocation_failed: ESP_LOGE(tag, "DH param malloc failed"); break;
        case BlufiSecurityDiagnostic::parameter_retained: ESP_LOGI(tag, "Recv DH param len: %d", first); break;
        case BlufiSecurityDiagnostic::missing_parameter: ESP_LOGE(tag, "dh_param is NULL"); break;
        case BlufiSecurityDiagnostic::short_parameter: ESP_LOGE(tag, "Invalid dh param len: need %d, got %d", first, second); break;
        case BlufiSecurityDiagnostic::dh_parse_failed: ESP_LOGE(tag, "dhm_read_params failed: %d", first); break;
        case BlufiSecurityDiagnostic::dh_length_unsupported: ESP_LOGE(tag, "dhm len %d not supported (max 128)", first); break;
        case BlufiSecurityDiagnostic::public_key_failed: ESP_LOGE(tag, "dhm_make_public failed: %d", first); break;
        case BlufiSecurityDiagnostic::shared_secret_failed: ESP_LOGE(tag, "dhm_calc_secret failed: %d", first); break;
        case BlufiSecurityDiagnostic::digest_failed: ESP_LOGE(tag, "mbedtls_md5 failed: %d", first); break;
        case BlufiSecurityDiagnostic::negotiation_complete: ESP_LOGI(tag, "DH negotiate done, pubkey len=%d", first); break;
    }
}

int BlufiCryptoAdapter::last_crypto_error() const {
    return last_crypto_error_;
}

std::optional<firmware::core::ByteVector>
BlufiCryptoAdapter::take_negotiation_response() {
    if (negotiation_response_.empty()) {
        return std::nullopt;
    }
    firmware::core::ByteVector response = std::move(negotiation_response_);
    negotiation_response_.clear();
    return response;
}

}  // namespace firmware::target
