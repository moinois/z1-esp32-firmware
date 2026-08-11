/** @file @brief Implements aggregate cleanup, read-failure classification, and validation. */
#include "application/update/update_validation.hpp"
#include "core/filesystem/sd_user_path.hpp"
#include "core/protocol/protocol_constants.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <algorithm>
#include <new>

namespace firmware::application {
namespace {

const std::string aggregate_path = core::physical_sd_path("/firmware.bin");
const std::string partial_aggregate_path =
    core::physical_sd_path("/firmware.bin.part");
constexpr std::array<std::uint8_t, 6U> reset_command{
    'r', 'e', 's', 'e', 't', 0U};

}  // namespace

UpdateBytes::UpdateBytes(core::ByteVector bytes) {
    auto allocated = allocate(bytes.size());
    if (!allocated.has_value()) return;
    *this = std::move(*allocated);
    std::copy(bytes.begin(), bytes.end(), data());
}

UpdateBytes::UpdateBytes(std::initializer_list<std::uint8_t> bytes)
    : UpdateBytes(core::ByteVector(bytes)) {}

std::optional<UpdateBytes> UpdateBytes::allocate(std::size_t size) {
    if (size == 0U) return UpdateBytes{};
    auto data = std::unique_ptr<std::uint8_t[]>(new (std::nothrow) std::uint8_t[size]);
    if (!data) return std::nullopt;
    return UpdateBytes(size, std::move(data));
}

UpdateValidationService::UpdateValidationService(UpdateValidationPort& port)
    : port_(port) {}

std::optional<ValidatedUpdatePackage> UpdateValidationService::validate(
    std::uint64_t now_milliseconds) {
    port_.remove_partial(partial_aggregate_path);
    port_.clear_attributes(aggregate_path);
    UpdateLoadResult loaded = port_.load_aggregate(aggregate_path);
    if (loaded.failure == UpdateLoadFailure::absent) {
        port_.send_controller_packet(
            core::protocol::general_command,
            core::BytesView(reset_command.data(), reset_command.size()));
        return std::nullopt;
    }

    port_.aggregate_opened();
    if (loaded.failure == UpdateLoadFailure::allocation) {
        port_.publish_error();
        return std::nullopt;
    }
    if (loaded.failure == UpdateLoadFailure::short_read) {
        port_.remove_aggregate(aggregate_path);
        port_.publish_error();
        return std::nullopt;
    }

    const core::UpdateParseResult parsed =
        core::parse_update_package(loaded.bytes);
    if (!parsed.valid()) {
        reject_format(now_milliseconds);
        return std::nullopt;
    }
    port_.report_valid_header(*parsed.header,
                              {loaded.bytes.data(), core::update_package_header_size});
    if (parsed.header->mainboard_size != 0U) {
        const core::BytesView image(
            loaded.bytes.data() + core::update_package_header_size,
            parsed.header->mainboard_size);
        if (!port_.valid_mainboard_image(image)) {
            reject_format(now_milliseconds);
            return std::nullopt;
        }
    }

    return ValidatedUpdatePackage{*parsed.header, std::move(loaded.bytes)};
}

void UpdateValidationService::reject_format(
    std::uint64_t now_milliseconds) {
    port_.broadcast_validation_error(now_milliseconds);
    port_.remove_aggregate(aggregate_path);
    port_.publish_error();
}

}  // namespace firmware::application
