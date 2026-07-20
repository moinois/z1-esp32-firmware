// Implements event-timed TPDO payload construction from mapped dictionary values.
#include "firmware/core/canopen_tpdo.hpp"

#include <cstddef>
#include <cstdint>

namespace firmware::core {
namespace {

constexpr std::uint8_t pdo_count = 4U;
constexpr std::uint8_t communication_identifier_subindex = 1U;
constexpr std::uint8_t transmission_type_subindex = 2U;
constexpr std::uint8_t event_timer_subindex = 5U;
constexpr std::uint8_t mapping_count_subindex = 0U;
constexpr std::uint8_t mapping_first_subindex = 1U;
constexpr std::uint8_t maximum_mapping_entries = 8U;
constexpr std::uint32_t disabled_identifier_mask = 0x80000000U;
constexpr std::uint32_t identifier_mask = 0x7ffU;
constexpr std::uint32_t cycle_milliseconds = 10U;

std::uint32_t read_le(const DictionaryReadResult& result) {
    std::uint32_t value = 0U;
    for (std::size_t index = 0U; index < result.data.size(); ++index) {
        value |= static_cast<std::uint32_t>(result.data[index]) << (index * 8U);
    }
    return value;
}

}  // namespace

CanopenTransmitPdoScheduler::CanopenTransmitPdoScheduler(
    const CanopenObjectDictionary& dictionary)
    : dictionary_(dictionary) {}

std::optional<CanFrame> CanopenTransmitPdoScheduler::process_cycle() {
    for (std::uint8_t pdo = 0U; pdo < pdo_count; ++pdo) {
        elapsed_milliseconds_[pdo] += cycle_milliseconds;
        if (const auto frame = build_tpdo(pdo); frame.has_value()) {
            elapsed_milliseconds_[pdo] = 0U;
            return frame;
        }
    }
    return std::nullopt;
}

std::optional<CanFrame> CanopenTransmitPdoScheduler::build_tpdo(
    std::uint8_t pdo_number) {
    const auto communication_index = static_cast<std::uint16_t>(
        canopen_object::first_tpdo_communication + pdo_number);
    const auto identifier = dictionary_.read(
        communication_index, communication_identifier_subindex);
    const auto transmission = dictionary_.read(
        communication_index, transmission_type_subindex);
    const auto timer = dictionary_.read(communication_index, event_timer_subindex);
    if (identifier.abort != SdoAbort::none || transmission.abort != SdoAbort::none ||
        timer.abort != SdoAbort::none || identifier.data.size() != 4U ||
        transmission.data.size() != 1U || timer.data.size() != 2U ||
        (read_le(identifier) & disabled_identifier_mask) != 0U ||
        read_le(timer) == 0U ||
        elapsed_milliseconds_[pdo_number] < read_le(timer) ||
        read_le(transmission) == 0U) {
        return std::nullopt;
    }

    const auto mapping_index = static_cast<std::uint16_t>(
        canopen_object::first_tpdo_mapping + pdo_number);
    const auto count = dictionary_.read(mapping_index, mapping_count_subindex);
    if (count.abort != SdoAbort::none || count.data.size() != 1U ||
        count.data[0] > maximum_mapping_entries) {
        return std::nullopt;
    }

    CanFrame frame;
    frame.identifier = static_cast<std::uint16_t>(read_le(identifier) & identifier_mask);
    for (std::uint8_t entry = 0U; entry < count.data[0]; ++entry) {
        const auto mapping = dictionary_.read(
            mapping_index, static_cast<std::uint8_t>(mapping_first_subindex + entry));
        if (mapping.abort != SdoAbort::none || mapping.data.size() != 4U) {
            return std::nullopt;
        }
        const std::uint32_t descriptor = read_le(mapping);
        const std::size_t width = (descriptor & 0xffU) / 8U;
        if (width == 0U || width > 4U || frame.size + width > frame.data.size()) {
            return std::nullopt;
        }
        const auto value = dictionary_.read(
            static_cast<std::uint16_t>(descriptor >> 16U),
            static_cast<std::uint8_t>(descriptor >> 8U));
        if (value.abort != SdoAbort::none || value.data.size() < width) {
            return std::nullopt;
        }
        for (std::size_t byte = 0U; byte < width; ++byte) {
            frame.data[frame.size + byte] = value.data[byte];
        }
        frame.size = static_cast<std::uint8_t>(frame.size + width);
    }
    return frame;
}

}  // namespace firmware::core
