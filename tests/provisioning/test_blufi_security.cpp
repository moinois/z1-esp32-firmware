// Verifies BLUFI negotiation, key derivation, readiness, and AES-IV policy.
#include "test.hpp"

#include "firmware/application/blufi_security.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <utility>
#include <vector>

using firmware::application::BlufiDhFailure;
using firmware::application::BlufiDhResult;
using firmware::application::BlufiSecurityContext;
using firmware::application::BlufiSecurityPort;
using firmware::core::ByteVector;

namespace {

// Supplies deterministic crypto outcomes and records every security operation.
class FakeBlufiSecurityPort final : public BlufiSecurityPort {
public:
    // Allocates the requested parameter buffer unless failure is configured.
    std::optional<ByteVector> allocate_parameter_buffer(
        std::size_t size) override {
        allocation_sizes.push_back(size);
        if (!allocation_succeeds) {
            return std::nullopt;
        }
        return ByteVector(size, 0U);
    }

    // Captures exact parameter bytes and returns the configured DH result.
    BlufiDhResult derive_diffie_hellman(
        firmware::core::BytesView parameters) override {
        derived_parameters.assign(parameters.begin(), parameters.end());
        return dh_result;
    }

    // Captures transformed secret bytes and returns the configured digest.
    std::optional<std::array<std::uint8_t, 16U>> md5(
        firmware::core::BytesView input) override {
        digest_input.assign(input.begin(), input.end());
        return digest;
    }

    // Captures the key, IV, input, and direction for one AES-CFB operation.
    std::optional<ByteVector> aes_cfb128(
        const std::array<std::uint8_t, 16U>& key,
        const std::array<std::uint8_t, 16U>& iv,
        firmware::core::BytesView input, bool encrypt) override {
        aes_key = key;
        aes_iv = iv;
        aes_input.assign(input.begin(), input.end());
        aes_encrypt = encrypt;
        return aes_output;
    }

    // Records the public-key negotiation response.
    void send_negotiation_response(firmware::core::BytesView response) override {
        responses.emplace_back(response.begin(), response.end());
    }

    // Records one exact BLUFI error value.
    void report_error(std::uint8_t error) override {
        errors.push_back(error);
    }

    bool allocation_succeeds = true;
    BlufiDhResult dh_result{BlufiDhFailure::none, {1U}, {2U}};
    std::optional<std::array<std::uint8_t, 16U>> digest =
        std::array<std::uint8_t, 16U>{};
    std::optional<ByteVector> aes_output = ByteVector{};
    std::vector<std::size_t> allocation_sizes;
    ByteVector derived_parameters;
    ByteVector digest_input;
    std::array<std::uint8_t, 16U> aes_key{};
    std::array<std::uint8_t, 16U> aes_iv{};
    ByteVector aes_input;
    bool aes_encrypt = false;
    std::vector<ByteVector> responses;
    std::vector<std::uint8_t> errors;
};

// Passes concise literal test bytes through the production non-owning view API.
void receive(BlufiSecurityContext& context,
             std::initializer_list<std::uint8_t> message) {
    context.receive_negotiation(ByteVector(message));
}

// Performs a complete parameter-length and parameter-data negotiation.
void negotiate(BlufiSecurityContext& context, const ByteVector& parameters) {
    const std::uint16_t size = static_cast<std::uint16_t>(parameters.size());
    receive(
        context,
        {0U, static_cast<std::uint8_t>(size >> 8U),
         static_cast<std::uint8_t>(size & 0xFFU)});
    ByteVector message{1U};
    message.insert(message.end(), parameters.begin(), parameters.end());
    while (message.size() < 3U) {
        message.push_back(0U);
    }
    context.receive_negotiation(message);
}

}  // namespace

TEST_CASE(blesec_001_short_messages_report_data_format_error) {
    FakeBlufiSecurityPort port;
    BlufiSecurityContext context(port);

    receive(context, {});
    receive(context, {0U});
    receive(context, {1U, 2U});

    REQUIRE_EQ(port.errors, std::vector<std::uint8_t>({9U, 9U, 9U}));
}

TEST_CASE(blesec_001_and_006_length_is_big_endian_and_ignores_trailing_bytes) {
    FakeBlufiSecurityPort port;
    BlufiSecurityContext context(port);

    receive(context, {0U, 0x01U, 0x02U, 0xFFU});

    REQUIRE_EQ(port.allocation_sizes, std::vector<std::size_t>({258U}));
    REQUIRE_EQ(context.parameter_size(), 258U);
}

TEST_CASE(blesec_006_zero_or_failed_allocation_leaves_no_parameter_buffer) {
    FakeBlufiSecurityPort port;
    BlufiSecurityContext context(port);

    receive(context, {0U, 0U, 0U});
    port.allocation_succeeds = false;
    receive(context, {0U, 0U, 4U});
    receive(context, {1U, 1U, 2U, 3U, 4U});

    REQUIRE_EQ(context.parameter_size(), 0U);
    REQUIRE_EQ(port.errors, std::vector<std::uint8_t>({5U, 5U, 6U}));
}

TEST_CASE(blesec_001_and_006_parameter_data_is_exact_and_ignores_extra_bytes) {
    FakeBlufiSecurityPort port;
    BlufiSecurityContext context(port);
    receive(context, {0U, 0U, 3U});

    receive(context, {1U, 4U, 5U});
    REQUIRE_EQ(port.errors, std::vector<std::uint8_t>({6U}));
    receive(context, {1U, 4U, 5U, 6U, 7U});

    REQUIRE_EQ(port.derived_parameters, ByteVector({4U, 5U, 6U}));
}

TEST_CASE(blesec_002_to_004_success_transforms_secret_and_returns_public_key) {
    FakeBlufiSecurityPort port;
    port.dh_result = {BlufiDhFailure::none, {9U, 8U}, {0U, 1U, 2U, 3U}};
    std::array<std::uint8_t, 16U> expected_key{};
    for (std::size_t index = 0U; index < expected_key.size(); ++index) {
        expected_key[index] = static_cast<std::uint8_t>(index + 1U);
    }
    port.digest = expected_key;
    BlufiSecurityContext context(port);

    negotiate(context, {3U, 4U, 5U});

    REQUIRE(context.ready());
    REQUIRE_EQ(port.digest_input,
               ByteVector({0x5AU, 0x30U, 0x5DU, 0x41U}));
    REQUIRE_EQ(port.responses, std::vector<ByteVector>({{9U, 8U}}));
}

TEST_CASE(blesec_002_derivation_failures_and_oversize_outputs_map_to_errors) {
    for (const auto& test : std::vector<std::pair<BlufiDhResult, std::uint8_t>>{
             {{BlufiDhFailure::parse, {}, {}}, 7U},
             {{BlufiDhFailure::public_key, {}, {}}, 8U},
             {{BlufiDhFailure::shared_secret, {}, {}}, 10U},
             {{BlufiDhFailure::none, ByteVector(129U, 1U), {1U}}, 8U},
             {{BlufiDhFailure::none, {1U}, ByteVector(129U, 1U)}, 10U},
         }) {
        FakeBlufiSecurityPort port;
        port.dh_result = test.first;
        BlufiSecurityContext context(port);

        negotiate(context, {1U, 2U});

        REQUIRE(!context.ready());
        REQUIRE_EQ(port.errors, std::vector<std::uint8_t>({test.second}));
    }
}

TEST_CASE(blesec_004_digest_failure_reports_ten_without_becoming_ready) {
    FakeBlufiSecurityPort port;
    port.digest = std::nullopt;
    BlufiSecurityContext context(port);

    negotiate(context, {1U, 2U});

    REQUIRE(!context.ready());
    REQUIRE_EQ(port.errors, std::vector<std::uint8_t>({10U}));
}

TEST_CASE(blesec_004_aes_uses_fresh_sequence_iv_and_derived_key) {
    FakeBlufiSecurityPort port;
    std::array<std::uint8_t, 16U> expected_key{};
    expected_key.fill(0xA5U);
    port.digest = expected_key;
    port.aes_output = ByteVector({7U, 8U});
    BlufiSecurityContext context(port);
    negotiate(context, {1U, 2U});

    const auto encrypted = context.crypt(0x42U, ByteVector({5U, 6U}), true);

    REQUIRE_EQ(encrypted, std::optional<ByteVector>({7U, 8U}));
    REQUIRE_EQ(port.aes_key, expected_key);
    REQUIRE_EQ(port.aes_iv[0], 0x42U);
    for (std::size_t index = 1U; index < port.aes_iv.size(); ++index) {
        REQUIRE_EQ(port.aes_iv[index], 0U);
    }
    REQUIRE(port.aes_encrypt);
}

TEST_CASE(blesec_006_readiness_clears_only_with_context_lifecycle) {
    FakeBlufiSecurityPort port;
    BlufiSecurityContext context(port);
    negotiate(context, {1U, 2U});
    REQUIRE(context.ready());

    receive(context, {0U, 0U, 0U});
    port.dh_result.failure = BlufiDhFailure::parse;
    negotiate(context, {3U, 4U});
    REQUIRE(context.ready());

    context.destroy();
    REQUIRE(!context.ready());
    context.create();
    REQUIRE(!context.ready());
}
