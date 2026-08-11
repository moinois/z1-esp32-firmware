// Verifies aggregate loading failures, validation order, and image delegation.
#include "test.hpp"

#include "application/update/update_validation.hpp"
#include "core/protocol/crc.hpp"
#include "core/update/update_package.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using firmware::application::UpdateLoadFailure;
using firmware::application::UpdateLoadResult;
using firmware::application::UpdateBytes;
using firmware::application::UpdateValidationPort;
using firmware::application::UpdateValidationService;
using firmware::core::ByteVector;

namespace {

// Writes one little-endian 32-bit value into a test package.
void put_le32(ByteVector& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned index = 0U; index < 4U; ++index) {
        bytes[offset + index] =
            static_cast<std::uint8_t>(value >> (8U * index));
    }
}

// Builds one valid mainboard-only aggregate for orchestration tests.
ByteVector valid_package() {
    ByteVector bytes(35U, 0U);
    put_le32(bytes, 0U, 0x4D5173EEUL);
    bytes[4] = 1U;
    bytes[5] = 32U;
    bytes[6] = 1U;
    put_le32(bytes, 8U, 3U);
    bytes[32] = 0xE9U;
    bytes[33] = 1U;
    bytes[34] = 2U;
    put_le32(bytes, 24U,
             firmware::core::crc32_iso_hdlc({bytes.data(), 24U}));
    put_le32(bytes, 28U, firmware::core::aggregate_file_crc(bytes));
    return bytes;
}

// Records exact filesystem, phase, controller, and image-validation actions.
class FakeUpdateValidationPort final : public UpdateValidationPort {
public:
    // Records best-effort removal of the partial aggregate.
    void remove_partial(std::string_view path) override {
        calls.emplace_back("remove-partial");
        paths.emplace_back(path);
    }

    // Records best-effort FAT attribute clearing before aggregate open.
    void clear_attributes(std::string_view path) override {
        calls.emplace_back("clear-attributes");
        paths.emplace_back(path);
    }

    // Returns the configured aggregate load outcome.
    UpdateLoadResult load_aggregate(std::string_view path) override {
        calls.emplace_back("load");
        paths.emplace_back(path);
        return std::move(load_result);
    }

    // Records that opening the aggregate clears prior persisted failure.
    void aggregate_opened() override {
        calls.emplace_back("opened");
    }

    // Records best-effort aggregate deletion after destructive errors.
    void remove_aggregate(std::string_view path) override {
        calls.emplace_back("remove-aggregate");
        paths.emplace_back(path);
    }

    // Records one volatile update-error publication.
    void publish_error() override {
        calls.emplace_back("publish-error");
    }

    // Records one rate-limited validation error request.
    void broadcast_validation_error(std::uint64_t now_milliseconds) override {
        calls.emplace_back("broadcast-error");
        broadcast_times.push_back(now_milliseconds);
    }

    // Records the exact controller reset packet.
    void send_controller_packet(std::uint8_t type,
                                firmware::core::BytesView payload) override {
        calls.emplace_back("controller-reset");
        controller_types.push_back(type);
        controller_payloads.emplace_back(payload.begin(), payload.end());
    }

    // Validates only the declared mainboard image bytes.
    bool valid_mainboard_image(firmware::core::BytesView image) override {
        calls.emplace_back("validate-image");
        validated_images.emplace_back(image.begin(), image.end());
        return image_is_valid;
    }

    void report_valid_header(const firmware::core::UpdateHeader&,
                             firmware::core::BytesView) override {
        calls.emplace_back("report-header");
    }

    UpdateLoadResult load_result{UpdateLoadFailure::absent, {}};
    bool image_is_valid = true;
    std::vector<std::string> calls;
    std::vector<std::string> paths;
    std::vector<std::uint64_t> broadcast_times;
    std::vector<std::uint8_t> controller_types;
    std::vector<ByteVector> controller_payloads;
    std::vector<ByteVector> validated_images;
};

}  // namespace

TEST_CASE(upd_020_update_buffer_allocation_is_explicit_and_contiguous) {
    auto empty = UpdateBytes::allocate(0U);
    auto bytes = UpdateBytes::allocate(3U);
    REQUIRE(empty.has_value());
    REQUIRE_EQ(empty->size(), 0U);
    REQUIRE(bytes.has_value());
    REQUIRE_EQ(bytes->size(), 3U);
    bytes->data()[1] = 0x5aU;
    REQUIRE_EQ(static_cast<firmware::core::BytesView>(*bytes)[1], 0x5aU);
}

TEST_CASE(upd_004_and_005_absent_aggregate_cleans_then_sends_exact_reset) {
    FakeUpdateValidationPort port;
    UpdateValidationService validation(port);

    REQUIRE(!validation.validate(10U).has_value());

    REQUIRE_EQ(port.calls,
               std::vector<std::string>({"remove-partial",
                                         "clear-attributes", "load",
                                         "controller-reset"}));
    REQUIRE_EQ(port.paths,
               std::vector<std::string>({"/sd/firmware.bin.part",
                                         "/sd/firmware.bin",
                                         "/sd/firmware.bin"}));
    REQUIRE_EQ(port.controller_types, std::vector<std::uint8_t>({0xA2U}));
    REQUIRE_EQ(port.controller_payloads,
               std::vector<ByteVector>({{'r', 'e', 's', 'e', 't', 0U}}));
}

TEST_CASE(upd_020_nondestructive_load_failures_publish_and_leave_aggregate) {
    for (const UpdateLoadFailure failure : {UpdateLoadFailure::allocation}) {
        FakeUpdateValidationPort port;
        port.load_result = {failure, {}};
        UpdateValidationService validation(port);

        REQUIRE(!validation.validate(20U).has_value());

        REQUIRE_EQ(port.calls.back(), std::string("publish-error"));
        REQUIRE_EQ(port.calls[3], std::string("opened"));
        REQUIRE(std::find(port.calls.begin(), port.calls.end(),
                          "remove-aggregate") == port.calls.end());
        REQUIRE(port.broadcast_times.empty());
    }
}

TEST_CASE(upd_021_short_read_deletes_then_publishes_without_format_broadcast) {
    FakeUpdateValidationPort port;
    port.load_result = {UpdateLoadFailure::short_read, {1U, 2U}};
    UpdateValidationService validation(port);

    REQUIRE(!validation.validate(30U).has_value());

    REQUIRE_EQ(port.calls,
               std::vector<std::string>({"remove-partial",
                                         "clear-attributes", "load", "opened",
                                         "remove-aggregate", "publish-error"}));
    REQUIRE(port.broadcast_times.empty());
}

TEST_CASE(upd_022_invalid_package_broadcasts_deletes_then_publishes) {
    FakeUpdateValidationPort port;
    port.load_result = {UpdateLoadFailure::none, ByteVector(32U, 0U)};
    UpdateValidationService validation(port);

    REQUIRE(!validation.validate(40U).has_value());

    REQUIRE_EQ(port.calls,
               std::vector<std::string>({"remove-partial",
                                         "clear-attributes", "load", "opened",
                                         "broadcast-error", "remove-aggregate",
                                         "publish-error"}));
    REQUIRE_EQ(port.broadcast_times, std::vector<std::uint64_t>({40U}));
}

TEST_CASE(upd_014_valid_package_delegates_exact_declared_mainboard_image) {
    FakeUpdateValidationPort port;
    port.load_result = {UpdateLoadFailure::none, valid_package()};
    UpdateValidationService validation(port);

    const auto package = validation.validate(50U);

    REQUIRE(package.has_value());
    REQUIRE_EQ(package->header.mainboard_size, 3U);
    REQUIRE_EQ(port.validated_images,
               std::vector<ByteVector>({{0xE9U, 1U, 2U}}));
    REQUIRE_EQ(port.calls[4], std::string("report-header"));
    REQUIRE_EQ(port.calls.back(), std::string("validate-image"));
}

TEST_CASE(upd_014_structurally_invalid_mainboard_uses_format_error_path) {
    FakeUpdateValidationPort port;
    port.load_result = {UpdateLoadFailure::none, valid_package()};
    port.image_is_valid = false;
    UpdateValidationService validation(port);

    REQUIRE(!validation.validate(60U).has_value());

    REQUIRE_EQ(port.calls[4], std::string("report-header"));
    REQUIRE_EQ(port.calls[5], std::string("validate-image"));
    REQUIRE_EQ(port.calls[6], std::string("broadcast-error"));
    REQUIRE_EQ(port.calls[7], std::string("remove-aggregate"));
    REQUIRE_EQ(port.calls[8], std::string("publish-error"));
}
