/** @file @brief Implements bounded DIAG-025 BLUFI custom-data records. */
#include "application/diagnostics/blufi_custom_diagnostics.hpp"

#include <algorithm>

namespace firmware::application {
namespace {

constexpr std::size_t maximum_logged_data_size = 65535U;
constexpr std::size_t bytes_per_record = 16U;
constexpr char hexadecimal_digits[] = "0123456789abcdef";

}  // namespace

std::string blufi_custom_data_length_message(std::size_t data_size) {
    return "Recv custom data, len=" + std::to_string(data_size);
}

std::optional<std::string> blufi_custom_data_hex_message(
    core::BytesView data, std::size_t offset) {
    const std::size_t bounded_size = std::min(data.size(), maximum_logged_data_size);
    if (offset >= bounded_size) {
        return std::nullopt;
    }
    const std::size_t count = std::min(bytes_per_record, bounded_size - offset);
    std::string message;
    message.reserve(count * 3U - 1U);
    for (std::size_t index = 0U; index < count; ++index) {
        if (index != 0U) {
            message.push_back(' ');
        }
        const std::uint8_t value = data[offset + index];
        message.push_back(hexadecimal_digits[value >> 4U]);
        message.push_back(hexadecimal_digits[value & 0x0fU]);
    }
    return message;
}

}  // namespace firmware::application
