// Declares the complete local CANopen object dictionary and SDO access policy.
#pragma once

#include "firmware/core/bytes.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace firmware::core {

namespace canopen_object {

inline constexpr std::uint16_t device_type = 0x1000U;
inline constexpr std::uint16_t error_register = 0x1001U;
inline constexpr std::uint16_t predefined_error = 0x1003U;
inline constexpr std::uint16_t sync_identifier = 0x1005U;
inline constexpr std::uint16_t sync_period = 0x1006U;
inline constexpr std::uint16_t sync_window = 0x1007U;
inline constexpr std::uint16_t store_parameters = 0x1010U;
inline constexpr std::uint16_t restore_parameters = 0x1011U;
inline constexpr std::uint16_t time_identifier = 0x1012U;
inline constexpr std::uint16_t emergency_identifier = 0x1014U;
inline constexpr std::uint16_t emergency_inhibit_time = 0x1015U;
inline constexpr std::uint16_t consumer_heartbeat = 0x1016U;
inline constexpr std::uint16_t producer_heartbeat = 0x1017U;
inline constexpr std::uint16_t identity = 0x1018U;
inline constexpr std::uint16_t sync_counter_overflow = 0x1019U;
inline constexpr std::uint16_t sdo_server = 0x1200U;
inline constexpr std::uint16_t sdo_client = 0x1280U;
inline constexpr std::uint16_t first_rpdo_communication = 0x1400U;
inline constexpr std::uint16_t first_rpdo_mapping = 0x1600U;
inline constexpr std::uint16_t first_tpdo_communication = 0x1800U;
inline constexpr std::uint16_t first_tpdo_mapping = 0x1a00U;
inline constexpr std::uint16_t digital_input = 0x6000U;
inline constexpr std::uint16_t digital_output = 0x6001U;

}  // namespace canopen_object

// Uses standard CANopen abort codes needed by local dictionary accesses.
enum class SdoAbort : std::uint32_t {
    none = 0x00000000U,
    command_specifier = 0x05040001U,
    read_only = 0x06010002U,
    object_not_found = 0x06020000U,
    pdo_mapping = 0x06040041U,
    type_mismatch = 0x06070010U,
    value_range = 0x06090030U,
    subindex_not_found = 0x06090011U,
};

// Describes SDO and PDO access without exposing dictionary storage.
struct ObjectPermissions {
    std::uint8_t size_bits = 0U;
    bool writable = false;
    bool tpdo_mappable = false;
    bool rpdo_mappable = false;
};

// Returns one little-endian dictionary value or its standard abort reason.
struct DictionaryReadResult {
    SdoAbort abort = SdoAbort::none;
    ByteVector data;
};

// Reports side effects which an outer CANopen service must apply after a write.
struct DictionaryWriteEffects {
    std::optional<std::uint16_t> producer_heartbeat_period;
    std::optional<std::uint32_t> digital_output;
};

// Returns the access outcome and any target-independent external effects.
struct DictionaryWriteResult {
    SdoAbort abort = SdoAbort::none;
    DictionaryWriteEffects effects;
};

// Stores every specified local object while remaining independent of an SDO stack.
class CanopenObjectDictionary {
public:
    // Creates every mutable object with its specified initial value.
    CanopenObjectDictionary();

    // Reads one exact object as a little-endian scalar.
    DictionaryReadResult read(std::uint16_t index,
                              std::uint8_t subindex) const;

    // Writes one exact object and validates access, size, and PDO mappings.
    DictionaryWriteResult write(std::uint16_t index,
                                std::uint8_t subindex,
                                BytesView data);

    // Returns metadata for one present entry, or no value when it is absent.
    std::optional<ObjectPermissions> permissions(
        std::uint16_t index, std::uint8_t subindex) const;

    // Retains every in-memory value across a communication-only reset.
    void communication_reset();

    // Updates the read-only error register from local CAN service state.
    void set_error_register(std::uint8_t value);

    // Stores one predefined error and extends the visible count as needed.
    void set_error_history(std::size_t slot, std::uint32_t value);

private:
    struct Entry {
        std::uint32_t value;
        ObjectPermissions permissions;
    };

    struct PdoCommunication {
        std::uint32_t identifier;
        std::uint8_t transmission_type;
        std::uint16_t inhibit_time;
        std::uint16_t event_timer;
        std::uint8_t sync_start;
    };

    // Finds one current value and its immutable access metadata.
    std::optional<Entry> entry(std::uint16_t index,
                               std::uint8_t subindex) const;

    // Reports whether an index exists even when its requested subindex does not.
    static bool index_supported(std::uint16_t index);

    // Assigns a value after common access checks have succeeded.
    DictionaryWriteEffects assign(std::uint16_t index,
                                  std::uint8_t subindex,
                                  std::uint32_t value);

    // Validates one RPDO or TPDO mapping descriptor against target metadata.
    bool valid_mapping(std::uint32_t descriptor, bool transmit) const;

    std::uint8_t error_register_ = 0U;
    std::array<std::uint32_t, 16U> error_history_{};
    std::uint8_t error_history_count_ = 0U;
    std::uint32_t sync_identifier_ = 0x00000080U;
    std::uint32_t sync_period_ = 0U;
    std::uint32_t sync_window_ = 0U;
    std::uint32_t time_identifier_ = 0x00000100U;
    std::uint32_t emergency_identifier_ = 0x00000080U;
    std::uint16_t emergency_inhibit_time_ = 0U;
    std::uint16_t heartbeat_period_ = 0U;
    std::uint8_t sync_counter_overflow_ = 0U;
    std::array<std::uint32_t, 4U> store_parameters_{};
    std::array<std::uint32_t, 4U> restore_parameters_{};
    std::array<std::uint32_t, 8U> consumer_heartbeats_{};
    std::uint32_t sdo_client_request_identifier_ = 0x80000000U;
    std::uint32_t sdo_client_response_identifier_ = 0x80000000U;
    std::uint8_t sdo_client_node_id_ = 0x01U;
    std::array<PdoCommunication, 4U> rpdo_communication_{};
    std::array<std::uint8_t, 4U> rpdo_mapping_count_{};
    std::array<std::array<std::uint32_t, 8U>, 4U> rpdo_mapping_{};
    std::array<PdoCommunication, 4U> tpdo_communication_{};
    std::array<std::uint8_t, 4U> tpdo_mapping_count_{};
    std::array<std::array<std::uint32_t, 8U>, 4U> tpdo_mapping_{};
    std::uint32_t digital_output_ = 0U;
};

}  // namespace firmware::core
