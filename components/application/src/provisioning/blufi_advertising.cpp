// Implements deterministic conditional packing for the 31-byte BLE AD limit.
#include "firmware/application/blufi_advertising.hpp"

#include <algorithm>
#include <initializer_list>

namespace firmware::application {
namespace {

constexpr std::string_view device_name_prefix = "MK_";
constexpr std::size_t maximum_machine_name_suffix_size = 23U;
constexpr std::size_t transmit_power_maximum_suffix_size = 20U;
constexpr std::size_t complete_service_maximum_suffix_size = 16U;
constexpr std::size_t incomplete_service_suffix_size = 17U;
constexpr std::size_t interval_maximum_suffix_size = 10U;

void append(core::ByteVector& destination,
            std::initializer_list<std::uint8_t> bytes) {
    destination.insert(destination.end(), bytes.begin(), bytes.end());
}

}  // namespace

std::string blufi_device_name(std::string_view machine_name) {
    std::string result(device_name_prefix);
    result.append(machine_name.substr(0U, maximum_machine_name_suffix_size));
    return result;
}

core::ByteVector blufi_advertising_data(std::string_view device_name,
                                        std::int8_t transmit_power_dbm) {
    const bool already_prefixed =
        device_name.size() >= device_name_prefix.size() &&
        device_name.substr(0U, device_name_prefix.size()) == device_name_prefix;
    const std::string normalized_name = blufi_device_name(
        already_prefixed ? device_name.substr(device_name_prefix.size())
                         : device_name);
    const std::size_t suffix_size = normalized_name.size() - device_name_prefix.size();
    core::ByteVector result;
    result.reserve(31U);
    append(result, {0x02U, 0x01U, 0x06U});
    result.push_back(static_cast<std::uint8_t>(suffix_size + 4U));
    result.push_back(0x09U);
    result.insert(result.end(), normalized_name.begin(), normalized_name.end());
    if (suffix_size <= transmit_power_maximum_suffix_size) {
        append(result, {0x02U, 0x0AU,
                        static_cast<std::uint8_t>(transmit_power_dbm)});
    }
    if (suffix_size <= complete_service_maximum_suffix_size) {
        append(result, {0x03U, 0x03U, 0xFFU, 0xFFU});
    } else if (suffix_size == incomplete_service_suffix_size) {
        append(result, {0x01U, 0x02U});
    }
    if (suffix_size <= interval_maximum_suffix_size) {
        append(result, {0x05U, 0x12U, 0x06U, 0x00U, 0x10U, 0x00U});
    }
    return result;
}

}  // namespace firmware::application
