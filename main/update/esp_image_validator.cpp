// Implements bounded ESP image header, segment, and checksum validation.
#include "esp_image_validator.hpp"

#include "esp_app_format.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace firmware::target {
namespace {

constexpr std::uint8_t image_checksum_seed = 0xEFU;

template <typename Value>
bool can_read(std::size_t offset, std::size_t image_size) {
    return offset <= image_size && sizeof(Value) <= image_size - offset;
}

}  // namespace

bool EspImageValidator::valid_mainboard_image(
    firmware::core::BytesView image) const {
    if (!can_read<esp_image_header_t>(0U, image.size())) {
        return false;
    }
    esp_image_header_t header{};
    std::memcpy(&header, image.data(), sizeof(header));
    if (header.magic != ESP_IMAGE_HEADER_MAGIC ||
        header.segment_count == 0U ||
        header.segment_count > ESP_IMAGE_MAX_SEGMENTS) {
        return false;
    }

    std::size_t offset = sizeof(esp_image_header_t);
    std::uint8_t checksum = image_checksum_seed;
    for (std::uint8_t index = 0U; index < header.segment_count; ++index) {
        if (!can_read<esp_image_segment_header_t>(offset, image.size())) {
            return false;
        }
        esp_image_segment_header_t segment{};
        std::memcpy(&segment, image.data() + offset, sizeof(segment));
        offset += sizeof(segment);
        if (segment.data_len > image.size() - offset) {
            return false;
        }
        for (std::size_t byte = 0U; byte < segment.data_len; ++byte) {
            checksum ^= image[offset + byte];
        }
        offset += segment.data_len;
    }
    return offset < image.size() && image[offset] == checksum;
}

}  // namespace firmware::target
