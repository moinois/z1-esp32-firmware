/** @file @brief Implements compact, table-shaped CANopen dictionary storage and validation. */
#include "firmware/core/canopen_dictionary.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace firmware::core {
namespace {

constexpr std::uint8_t bits_u8 = 8U;
constexpr std::uint8_t bits_u16 = 16U;
constexpr std::uint8_t bits_u32 = 32U;
constexpr std::uint8_t bytes_per_bit_group = 8U;
constexpr std::uint8_t maximum_mapping_entries = 8U;
constexpr std::uint8_t parameter_subindex_count = 4U;
constexpr std::uint8_t consumer_heartbeat_count = 8U;
constexpr std::uint8_t identity_subindex_count = 4U;
constexpr std::uint8_t sdo_server_subindex_count = 2U;
constexpr std::uint8_t sdo_client_subindex_count = 3U;
constexpr std::uint8_t rpdo_communication_subindex_count = 5U;
constexpr std::uint8_t tpdo_communication_subindex_count = 6U;
constexpr std::uint32_t device_type = 0U;
constexpr std::uint32_t sdo_server_request_base_identifier = 0x600U;
constexpr std::uint32_t sdo_server_response_base_identifier = 0x580U;
constexpr std::uint32_t disabled_rpdo_first_identifier = 0x80000200U;
constexpr std::uint32_t disabled_tpdo_first_identifier = 0xc0000180U;
constexpr std::uint32_t pdo_identifier_step = 0x100U;
constexpr std::uint8_t asynchronous_transmission_type = 0xfeU;

// Constructs concise access metadata for an unsigned scalar.
ObjectPermissions access(std::uint8_t size_bits,
                         bool writable = false,
                         bool tpdo_mappable = false,
                         bool rpdo_mappable = false) {
    return {size_bits, writable, tpdo_mappable, rpdo_mappable};
}

// Encodes an unsigned scalar using the little-endian CANopen byte order.
ByteVector encode_little_endian(std::uint32_t value, std::uint8_t size_bits) {
    const std::size_t size = size_bits / bytes_per_bit_group;
    ByteVector data;
    data.reserve(size);
    for (std::size_t offset = 0U; offset < size; ++offset) {
        data.push_back(static_cast<std::uint8_t>(
            value >> (bytes_per_bit_group * offset)));
    }
    return data;
}

// Decodes an already size-validated little-endian unsigned scalar.
std::uint32_t decode_little_endian(BytesView data) {
    std::uint32_t value = 0U;
    for (std::size_t offset = 0U; offset < data.size(); ++offset) {
        value |= static_cast<std::uint32_t>(data[offset]) <<
                 (bytes_per_bit_group * offset);
    }
    return value;
}

// Reports whether an index belongs to one inclusive four-object PDO range.
bool in_four_object_range(std::uint16_t index, std::uint16_t first) {
    return index >= first && index < first + 4U;
}

}  // namespace

CanopenObjectDictionary::CanopenObjectDictionary() {
    store_parameters_.fill(1U);
    restore_parameters_.fill(1U);
    for (std::size_t offset = 0U; offset < rpdo_communication_.size();
         ++offset) {
        rpdo_communication_[offset] = {
            disabled_rpdo_first_identifier +
                pdo_identifier_step * static_cast<std::uint32_t>(offset),
            asynchronous_transmission_type,
            0U,
            0U,
            0U};
        tpdo_communication_[offset] = {
            disabled_tpdo_first_identifier +
                pdo_identifier_step * static_cast<std::uint32_t>(offset),
            asynchronous_transmission_type,
            0U,
            0U,
            0U};
    }
}

DictionaryReadResult CanopenObjectDictionary::read(
    std::uint16_t index, std::uint8_t subindex) const {
    const std::optional<Entry> found = entry(index, subindex);
    if (!found.has_value()) {
        return {index_supported(index) ? SdoAbort::subindex_not_found
                                       : SdoAbort::object_not_found,
                {}};
    }
    return {SdoAbort::none,
            encode_little_endian(found->value,
                                 found->permissions.size_bits)};
}

DictionaryWriteResult CanopenObjectDictionary::write(
    std::uint16_t index, std::uint8_t subindex, BytesView data) {
    const std::optional<Entry> found = entry(index, subindex);
    if (!found.has_value()) {
        return {index_supported(index) ? SdoAbort::subindex_not_found
                                       : SdoAbort::object_not_found,
                {}};
    }
    if (!found->permissions.writable) {
        return {SdoAbort::read_only, {}};
    }
    if (data.size() != found->permissions.size_bits / bytes_per_bit_group) {
        return {SdoAbort::type_mismatch, {}};
    }

    const std::uint32_t value = decode_little_endian(data);
    if (index == canopen_object::predefined_error && subindex == 0U &&
        value != 0U) {
        return {SdoAbort::value_range, {}};
    }
    if ((in_four_object_range(index, canopen_object::first_rpdo_mapping) ||
         in_four_object_range(index, canopen_object::first_tpdo_mapping)) &&
        subindex == 0U && value > maximum_mapping_entries) {
        return {SdoAbort::value_range, {}};
    }
    if (in_four_object_range(index, canopen_object::first_rpdo_mapping) &&
        subindex != 0U &&
        !valid_mapping(value, false)) {
        return {SdoAbort::pdo_mapping, {}};
    }
    if (in_four_object_range(index, canopen_object::first_tpdo_mapping) &&
        subindex != 0U &&
        !valid_mapping(value, true)) {
        return {SdoAbort::pdo_mapping, {}};
    }
    return {SdoAbort::none, assign(index, subindex, value)};
}

std::optional<ObjectPermissions> CanopenObjectDictionary::permissions(
    std::uint16_t index, std::uint8_t subindex) const {
    const std::optional<Entry> found = entry(index, subindex);
    if (!found.has_value()) {
        return std::nullopt;
    }
    return found->permissions;
}

void CanopenObjectDictionary::communication_reset() {}

void CanopenObjectDictionary::set_error_register(std::uint8_t value) {
    error_register_ = value;
}

void CanopenObjectDictionary::set_error_history(std::size_t slot,
                                                std::uint32_t value) {
    if (slot >= error_history_.size()) {
        return;
    }
    error_history_[slot] = value;
    error_history_count_ = std::max<std::uint8_t>(
        error_history_count_, static_cast<std::uint8_t>(slot + 1U));
}

std::optional<CanopenObjectDictionary::Entry>
CanopenObjectDictionary::entry(std::uint16_t index,
                               std::uint8_t subindex) const {
    switch (index) {
        case canopen_object::device_type:
            return subindex == 0U
                       ? std::optional<Entry>{{device_type, access(bits_u32)}}
                       : std::nullopt;
        case canopen_object::error_register:
            return subindex == 0U
                       ? std::optional<Entry>{{error_register_,
                                               access(bits_u8, false, true)}}
                       : std::nullopt;
        case canopen_object::predefined_error:
            if (subindex == 0U) {
                return Entry{error_history_count_, access(bits_u8, true)};
            }
            if (subindex <= error_history_.size()) {
                return Entry{error_history_[subindex - 1U], access(bits_u32)};
            }
            return std::nullopt;
        case canopen_object::sync_identifier:
            return subindex == 0U
                       ? std::optional<Entry>{{sync_identifier_,
                                               access(bits_u32, true)}}
                       : std::nullopt;
        case canopen_object::sync_period:
            return subindex == 0U
                       ? std::optional<Entry>{{sync_period_,
                                               access(bits_u32, true)}}
                       : std::nullopt;
        case canopen_object::sync_window:
            return subindex == 0U
                       ? std::optional<Entry>{{sync_window_,
                                               access(bits_u32, true)}}
                       : std::nullopt;
        case canopen_object::time_identifier:
            return subindex == 0U
                       ? std::optional<Entry>{{time_identifier_,
                                               access(bits_u32, true)}}
                       : std::nullopt;
        case canopen_object::emergency_identifier:
            return subindex == 0U
                       ? std::optional<Entry>{{emergency_identifier_,
                                               access(bits_u32, true)}}
                       : std::nullopt;
        case canopen_object::emergency_inhibit_time:
            return subindex == 0U
                       ? std::optional<Entry>{{emergency_inhibit_time_,
                                               access(bits_u16, true)}}
                       : std::nullopt;
        case canopen_object::producer_heartbeat:
            return subindex == 0U
                       ? std::optional<Entry>{{heartbeat_period_,
                                               access(bits_u16, true)}}
                       : std::nullopt;
        case canopen_object::sync_counter_overflow:
            return subindex == 0U
                       ? std::optional<Entry>{{sync_counter_overflow_,
                                               access(bits_u8, true)}}
                       : std::nullopt;
        default:
            break;
    }

    if (index == canopen_object::store_parameters ||
        index == canopen_object::restore_parameters) {
        if (subindex == 0U) {
            return Entry{parameter_subindex_count, access(bits_u8)};
        }
        if (subindex <= parameter_subindex_count) {
            const auto& values =
                index == canopen_object::store_parameters
                    ? store_parameters_
                    : restore_parameters_;
            return Entry{values[subindex - 1U], access(bits_u32, true)};
        }
        return std::nullopt;
    }
    if (index == canopen_object::consumer_heartbeat) {
        if (subindex == 0U) {
            return Entry{consumer_heartbeat_count, access(bits_u8)};
        }
        if (subindex <= consumer_heartbeat_count) {
            return Entry{consumer_heartbeats_[subindex - 1U],
                         access(bits_u32, true)};
        }
        return std::nullopt;
    }
    if (index == canopen_object::identity) {
        if (subindex == 0U) {
            return Entry{identity_subindex_count, access(bits_u8)};
        }
        return subindex <= identity_subindex_count
                   ? std::optional<Entry>{{0U, access(bits_u32)}}
                   : std::nullopt;
    }
    if (index == canopen_object::sdo_server) {
        if (subindex == 0U) {
            return Entry{sdo_server_subindex_count, access(bits_u8)};
        }
        if (subindex == 1U) {
            return Entry{sdo_server_request_base_identifier,
                         access(bits_u32, false, true)};
        }
        if (subindex == 2U) {
            return Entry{sdo_server_response_base_identifier,
                         access(bits_u32, false, true)};
        }
        return std::nullopt;
    }
    if (index == canopen_object::sdo_client) {
        if (subindex == 0U) {
            return Entry{sdo_client_subindex_count, access(bits_u8)};
        }
        if (subindex == 1U) {
            return Entry{sdo_client_request_identifier_,
                         access(bits_u32, true, true, true)};
        }
        if (subindex == 2U) {
            return Entry{sdo_client_response_identifier_,
                         access(bits_u32, true, true, true)};
        }
        return subindex == 3U
                   ? std::optional<Entry>{{sdo_client_node_id_,
                                           access(bits_u8, true)}}
                   : std::nullopt;
    }

    if (in_four_object_range(index,
                             canopen_object::first_rpdo_communication)) {
        const PdoCommunication& pdo = rpdo_communication_[
            index - canopen_object::first_rpdo_communication];
        switch (subindex) {
            case 0U:
                return Entry{rpdo_communication_subindex_count,
                             access(bits_u8)};
            case 1U:
                return Entry{pdo.identifier, access(bits_u32, true)};
            case 2U:
                return Entry{pdo.transmission_type, access(bits_u8, true)};
            case 5U:
                return Entry{pdo.event_timer, access(bits_u16, true)};
            default:
                return std::nullopt;
        }
    }
    if (in_four_object_range(index, canopen_object::first_rpdo_mapping)) {
        const std::size_t offset =
            index - canopen_object::first_rpdo_mapping;
        if (subindex == 0U) {
            return Entry{rpdo_mapping_count_[offset], access(bits_u8, true)};
        }
        return subindex <= maximum_mapping_entries
                   ? std::optional<Entry>{{rpdo_mapping_[offset][subindex - 1U],
                                           access(bits_u32, true, true, true)}}
                   : std::nullopt;
    }
    if (in_four_object_range(index,
                             canopen_object::first_tpdo_communication)) {
        const PdoCommunication& pdo = tpdo_communication_[
            index - canopen_object::first_tpdo_communication];
        switch (subindex) {
            case 0U:
                return Entry{tpdo_communication_subindex_count,
                             access(bits_u8)};
            case 1U:
                return Entry{pdo.identifier, access(bits_u32, true)};
            case 2U:
                return Entry{pdo.transmission_type, access(bits_u8, true)};
            case 3U:
                return Entry{pdo.inhibit_time, access(bits_u16, true)};
            case 5U:
                return Entry{pdo.event_timer, access(bits_u16, true)};
            case 6U:
                return Entry{pdo.sync_start, access(bits_u8, true)};
            default:
                return std::nullopt;
        }
    }
    if (in_four_object_range(index, canopen_object::first_tpdo_mapping)) {
        const std::size_t offset =
            index - canopen_object::first_tpdo_mapping;
        if (subindex == 0U) {
            return Entry{tpdo_mapping_count_[offset], access(bits_u8, true)};
        }
        return subindex <= maximum_mapping_entries
                   ? std::optional<Entry>{{tpdo_mapping_[offset][subindex - 1U],
                                           access(bits_u32, true, true, true)}}
                   : std::nullopt;
    }
    if (index == canopen_object::digital_input ||
        index == canopen_object::digital_output) {
        if (subindex == 0U) {
            return Entry{1U, access(bits_u8)};
        }
        if (subindex != 1U) {
            return std::nullopt;
        }
        if (index == canopen_object::digital_input) {
            return Entry{0U, access(bits_u32, false, true)};
        }
        return Entry{digital_output_, access(bits_u32, true, false, true)};
    }
    return std::nullopt;
}

bool CanopenObjectDictionary::index_supported(std::uint16_t index) {
    switch (index) {
        case canopen_object::device_type:
        case canopen_object::error_register:
        case canopen_object::predefined_error:
        case canopen_object::sync_identifier:
        case canopen_object::sync_period:
        case canopen_object::sync_window:
        case canopen_object::store_parameters:
        case canopen_object::restore_parameters:
        case canopen_object::time_identifier:
        case canopen_object::emergency_identifier:
        case canopen_object::emergency_inhibit_time:
        case canopen_object::consumer_heartbeat:
        case canopen_object::producer_heartbeat:
        case canopen_object::identity:
        case canopen_object::sync_counter_overflow:
        case canopen_object::sdo_server:
        case canopen_object::sdo_client:
        case canopen_object::digital_input:
        case canopen_object::digital_output:
            return true;
        default:
            return in_four_object_range(
                       index, canopen_object::first_rpdo_communication) ||
                   in_four_object_range(
                       index, canopen_object::first_rpdo_mapping) ||
                   in_four_object_range(
                       index, canopen_object::first_tpdo_communication) ||
                   in_four_object_range(
                       index, canopen_object::first_tpdo_mapping);
    }
}

DictionaryWriteEffects CanopenObjectDictionary::assign(
    std::uint16_t index, std::uint8_t subindex, std::uint32_t value) {
    DictionaryWriteEffects effects;
    switch (index) {
        case canopen_object::predefined_error:
            error_history_.fill(0U);
            error_history_count_ = 0U;
            break;
        case canopen_object::sync_identifier:
            sync_identifier_ = value;
            break;
        case canopen_object::sync_period:
            sync_period_ = value;
            break;
        case canopen_object::sync_window:
            sync_window_ = value;
            break;
        case canopen_object::time_identifier:
            time_identifier_ = value;
            break;
        case canopen_object::emergency_identifier:
            emergency_identifier_ = value;
            break;
        case canopen_object::emergency_inhibit_time:
            emergency_inhibit_time_ = static_cast<std::uint16_t>(value);
            break;
        case canopen_object::producer_heartbeat:
            heartbeat_period_ = static_cast<std::uint16_t>(value);
            effects.producer_heartbeat_period = heartbeat_period_;
            break;
        case canopen_object::sync_counter_overflow:
            sync_counter_overflow_ = static_cast<std::uint8_t>(value);
            break;
        case canopen_object::store_parameters:
            store_parameters_[subindex - 1U] = value;
            break;
        case canopen_object::restore_parameters:
            restore_parameters_[subindex - 1U] = value;
            break;
        case canopen_object::consumer_heartbeat:
            consumer_heartbeats_[subindex - 1U] = value;
            break;
        case canopen_object::sdo_client:
            if (subindex == 1U) {
                sdo_client_request_identifier_ = value;
            } else if (subindex == 2U) {
                sdo_client_response_identifier_ = value;
            } else {
                sdo_client_node_id_ = static_cast<std::uint8_t>(value);
            }
            break;
        case canopen_object::digital_output:
            digital_output_ = value;
            effects.digital_output = value;
            break;
        default:
            if (in_four_object_range(
                    index, canopen_object::first_rpdo_communication)) {
                PdoCommunication& pdo = rpdo_communication_[
                    index - canopen_object::first_rpdo_communication];
                if (subindex == 1U) {
                    pdo.identifier = value;
                } else if (subindex == 2U) {
                    pdo.transmission_type = static_cast<std::uint8_t>(value);
                } else {
                    pdo.event_timer = static_cast<std::uint16_t>(value);
                }
            } else if (in_four_object_range(
                           index, canopen_object::first_rpdo_mapping)) {
                const std::size_t offset =
                    index - canopen_object::first_rpdo_mapping;
                if (subindex == 0U) {
                    rpdo_mapping_count_[offset] =
                        static_cast<std::uint8_t>(value);
                } else {
                    rpdo_mapping_[offset][subindex - 1U] = value;
                }
            } else if (in_four_object_range(
                           index, canopen_object::first_tpdo_communication)) {
                PdoCommunication& pdo = tpdo_communication_[
                    index - canopen_object::first_tpdo_communication];
                if (subindex == 1U) {
                    pdo.identifier = value;
                } else if (subindex == 2U) {
                    pdo.transmission_type = static_cast<std::uint8_t>(value);
                } else if (subindex == 3U) {
                    pdo.inhibit_time = static_cast<std::uint16_t>(value);
                } else if (subindex == 5U) {
                    pdo.event_timer = static_cast<std::uint16_t>(value);
                } else {
                    pdo.sync_start = static_cast<std::uint8_t>(value);
                }
            } else if (in_four_object_range(
                           index, canopen_object::first_tpdo_mapping)) {
                const std::size_t offset =
                    index - canopen_object::first_tpdo_mapping;
                if (subindex == 0U) {
                    tpdo_mapping_count_[offset] =
                        static_cast<std::uint8_t>(value);
                } else {
                    tpdo_mapping_[offset][subindex - 1U] = value;
                }
            }
            break;
    }
    return effects;
}

bool CanopenObjectDictionary::valid_mapping(std::uint32_t descriptor,
                                            bool transmit) const {
    if (descriptor == 0U) {
        return true;
    }
    const std::uint16_t index = static_cast<std::uint16_t>(descriptor >> 16U);
    const std::uint8_t subindex =
        static_cast<std::uint8_t>(descriptor >> 8U);
    const std::uint8_t size_bits = static_cast<std::uint8_t>(descriptor);
    const std::optional<ObjectPermissions> target = permissions(index, subindex);
    if (!target.has_value() || target->size_bits != size_bits) {
        return false;
    }
    return transmit ? target->tpdo_mappable : target->rpdo_mappable;
}

}  // namespace firmware::core
