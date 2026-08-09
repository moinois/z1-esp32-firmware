/** @file @brief Implements BLUFI DH negotiation, salted key derivation, and AES-CFB policy. */
#include "application/provisioning/blufi_security.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace firmware::application {
namespace {

constexpr std::uint8_t parameter_length_kind = 0U;
constexpr std::uint8_t parameter_data_kind = 1U;
constexpr std::size_t minimum_negotiation_message_size = 3U;
constexpr std::size_t maximum_key_material_size = 128U;
constexpr std::uint8_t allocation_error = 5U;
constexpr std::uint8_t parameter_error = 6U;
constexpr std::uint8_t parameter_parse_error = 7U;
constexpr std::uint8_t public_key_error = 8U;
constexpr std::uint8_t data_format_error = 9U;
constexpr std::uint8_t key_digest_error = 10U;
constexpr std::array<std::uint8_t, 32U> shared_secret_salt{
    0x5AU, 0x31U, 0x5FU, 0x42U, 0x4CU, 0x55U, 0x46U, 0x49U,
    0x5FU, 0x53U, 0x41U, 0x4CU, 0x54U, 0x5FU, 0x32U, 0x30U,
    0x32U, 0x35U, 0xAAU, 0xB1U, 0xA3U, 0x06U, 0x88U, 0x45U,
    0x36U, 0x67U, 0x90U, 0x87U, 0x21U, 0x70U, 0x11U, 0x82U,
};

// Maps a crypto adapter's DH failure stage to its BLUFI error value.
std::uint8_t dh_error(BlufiDhFailure failure) {
    switch (failure) {
        case BlufiDhFailure::parse:
            return parameter_parse_error;
        case BlufiDhFailure::public_key:
            return public_key_error;
        case BlufiDhFailure::shared_secret:
            return key_digest_error;
        case BlufiDhFailure::none:
            break;
    }
    return key_digest_error;
}

}  // namespace

BlufiSecurityContext::BlufiSecurityContext(BlufiSecurityPort& port)
    : port_(port) {}

void BlufiSecurityContext::create() {
    parameters_.clear();
    key_.fill(0U);
    ready_ = false;
}

void BlufiSecurityContext::destroy() {
    create();
}

void BlufiSecurityContext::receive_negotiation(core::BytesView message) {
    if (message.size() < minimum_negotiation_message_size) {
        port_.report_error(data_format_error);
        return;
    }
    if (message[0] == parameter_length_kind) {
        receive_parameter_length(message);
    } else if (message[0] == parameter_data_kind) {
        receive_parameter_data(message);
    }
}

void BlufiSecurityContext::receive_parameter_length(core::BytesView message) {
    const std::size_t size =
        (static_cast<std::size_t>(message[1]) << 8U) | message[2];
    parameters_.clear();
    if (size == 0U) {
        port_.report_error(allocation_error);
        return;
    }
    auto allocated = port_.allocate_parameter_buffer(size);
    if (!allocated.has_value() || allocated->size() != size) {
        port_.report_error(allocation_error);
        return;
    }
    parameters_ = std::move(*allocated);
}

void BlufiSecurityContext::receive_parameter_data(core::BytesView message) {
    if (parameters_.empty() || message.size() - 1U < parameters_.size()) {
        port_.report_error(parameter_error);
        return;
    }
    std::copy_n(message.begin() + 1U, parameters_.size(), parameters_.begin());
    const BlufiDhResult derived = port_.derive_diffie_hellman(parameters_);
    if (derived.failure != BlufiDhFailure::none) {
        port_.report_error(dh_error(derived.failure));
        return;
    }
    if (derived.public_key.size() > maximum_key_material_size) {
        port_.report_error(public_key_error);
        return;
    }
    if (derived.shared_secret.size() > maximum_key_material_size) {
        port_.report_error(key_digest_error);
        return;
    }

    core::ByteVector transformed = derived.shared_secret;
    for (std::size_t index = 0U; index < transformed.size(); ++index) {
        transformed[index] ^= shared_secret_salt[index % shared_secret_salt.size()];
    }
    const auto digest = port_.md5(transformed);
    if (!digest.has_value()) {
        port_.report_error(key_digest_error);
        return;
    }
    key_ = *digest;
    ready_ = true;
    port_.send_negotiation_response(derived.public_key);
}

bool BlufiSecurityContext::ready() const {
    return ready_;
}

std::size_t BlufiSecurityContext::parameter_size() const {
    return parameters_.size();
}

std::optional<core::ByteVector> BlufiSecurityContext::crypt(
    std::uint8_t sequence, core::BytesView input, bool encrypt) {
    if (!ready_) {
        return std::nullopt;
    }
    std::array<std::uint8_t, 16U> iv{};
    iv[0] = sequence;
    return port_.aes_cfb128(key_, iv, input, encrypt);
}

}  // namespace firmware::application
