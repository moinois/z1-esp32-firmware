// Tests the complete local CANopen object dictionary and access policy.
#include "test.hpp"

#include "firmware/core/canopen_dictionary.hpp"
#include "firmware/core/canopen_node.hpp"
#include "firmware/core/canopen_pdo.hpp"
#include "firmware/core/canopen_tpdo.hpp"

#include <cstdint>

using firmware::core::ByteVector;
using firmware::core::CanopenObjectDictionary;
using firmware::core::CanopenReceivePdoRouter;
using firmware::core::CanopenTransmitPdoScheduler;
using firmware::core::SdoAbort;

namespace {

// Encodes one unsigned scalar in CANopen little-endian order.
ByteVector le(std::uint32_t value, std::size_t size) {
    ByteVector bytes;
    for (std::size_t index = 0U; index < size; ++index) {
        bytes.push_back(static_cast<std::uint8_t>(value >> (8U * index)));
    }
    return bytes;
}

// Reads one required dictionary scalar as an unsigned host value.
std::uint32_t read_value(const CanopenObjectDictionary& dictionary,
                         std::uint16_t index,
                         std::uint8_t subindex) {
    const auto result = dictionary.read(index, subindex);
    REQUIRE_EQ(result.abort, SdoAbort::none);
    std::uint32_t value = 0U;
    for (std::size_t offset = 0U; offset < result.data.size(); ++offset) {
        value |= static_cast<std::uint32_t>(result.data[offset]) <<
                 (8U * offset);
    }
    return value;
}

}  // namespace

TEST_CASE(od_001_scalars_use_exact_little_endian_sizes) {
    CanopenObjectDictionary dictionary;

    REQUIRE_EQ(dictionary.read(0x1000U, 0U).data, le(0U, 4U));
    REQUIRE_EQ(dictionary.read(0x1001U, 0U).data, le(0U, 1U));
    REQUIRE_EQ(dictionary.read(0x1015U, 0U).data, le(0U, 2U));
    REQUIRE_EQ(dictionary.read(0x1017U, 0U).data, le(0U, 2U));
}

TEST_CASE(od_010_communication_scalars_have_exact_initial_values) {
    CanopenObjectDictionary dictionary;

    REQUIRE_EQ(read_value(dictionary, 0x1005U, 0U), 0x00000080U);
    REQUIRE_EQ(read_value(dictionary, 0x1012U, 0U), 0x00000100U);
    REQUIRE_EQ(read_value(dictionary, 0x1014U, 0U), 0x00000080U);
    REQUIRE_EQ(read_value(dictionary, 0x1019U, 0U), 0U);
}

TEST_CASE(od_011_to_014_array_and_identity_shapes_are_complete) {
    CanopenObjectDictionary dictionary;

    REQUIRE_EQ(read_value(dictionary, 0x1003U, 0U), 0U);
    REQUIRE_EQ(read_value(dictionary, 0x1010U, 0U), 4U);
    REQUIRE_EQ(read_value(dictionary, 0x1010U, 4U), 1U);
    REQUIRE_EQ(read_value(dictionary, 0x1011U, 4U), 1U);
    REQUIRE_EQ(read_value(dictionary, 0x1016U, 0U), 8U);
    REQUIRE_EQ(read_value(dictionary, 0x1016U, 8U), 0U);
    REQUIRE_EQ(read_value(dictionary, 0x1018U, 0U), 4U);
    REQUIRE_EQ(read_value(dictionary, 0x1018U, 4U), 0U);
}

TEST_CASE(od_020_and_021_sdo_objects_have_exact_values_and_access) {
    CanopenObjectDictionary dictionary;

    REQUIRE_EQ(read_value(dictionary, 0x1200U, 0U), 2U);
    REQUIRE_EQ(read_value(dictionary, 0x1200U, 1U), 0x600U);
    REQUIRE_EQ(read_value(dictionary, 0x1200U, 2U), 0x580U);
    REQUIRE_EQ(read_value(dictionary, 0x1280U, 0U), 3U);
    REQUIRE_EQ(read_value(dictionary, 0x1280U, 1U), 0x80000000U);
    REQUIRE_EQ(read_value(dictionary, 0x1280U, 3U), 1U);
}

TEST_CASE(od_030_and_031_all_rpdo_defaults_are_present) {
    CanopenObjectDictionary dictionary;

    for (std::uint16_t offset = 0U; offset < 4U; ++offset) {
        REQUIRE_EQ(read_value(dictionary, 0x1400U + offset, 0U), 5U);
        REQUIRE_EQ(read_value(dictionary, 0x1400U + offset, 1U),
                   0x80000200U + 0x100U * offset);
        REQUIRE_EQ(read_value(dictionary, 0x1400U + offset, 2U), 0xfeU);
        REQUIRE_EQ(read_value(dictionary, 0x1400U + offset, 5U), 0U);
        REQUIRE_EQ(read_value(dictionary, 0x1600U + offset, 0U), 0U);
        REQUIRE_EQ(read_value(dictionary, 0x1600U + offset, 8U), 0U);
    }
}

TEST_CASE(od_040_and_041_all_tpdo_defaults_are_present) {
    CanopenObjectDictionary dictionary;

    for (std::uint16_t offset = 0U; offset < 4U; ++offset) {
        REQUIRE_EQ(read_value(dictionary, 0x1800U + offset, 0U), 6U);
        REQUIRE_EQ(read_value(dictionary, 0x1800U + offset, 1U),
                   0xc0000180U + 0x100U * offset);
        REQUIRE_EQ(read_value(dictionary, 0x1800U + offset, 2U), 0xfeU);
        REQUIRE_EQ(read_value(dictionary, 0x1800U + offset, 3U), 0U);
        REQUIRE_EQ(read_value(dictionary, 0x1800U + offset, 5U), 0U);
        REQUIRE_EQ(read_value(dictionary, 0x1800U + offset, 6U), 0U);
        REQUIRE_EQ(read_value(dictionary, 0x1a00U + offset, 0U), 0U);
        REQUIRE_EQ(read_value(dictionary, 0x1a00U + offset, 8U), 0U);
    }
}

TEST_CASE(od_050_and_051_digital_io_values_and_permissions_are_exact) {
    CanopenObjectDictionary dictionary;

    REQUIRE_EQ(read_value(dictionary, 0x6000U, 0U), 1U);
    REQUIRE_EQ(read_value(dictionary, 0x6000U, 1U), 0U);
    REQUIRE(dictionary.permissions(0x6000U, 1U)->tpdo_mappable);
    REQUIRE(!dictionary.permissions(0x6000U, 1U)->writable);
    REQUIRE_EQ(read_value(dictionary, 0x6001U, 0U), 1U);
    REQUIRE(dictionary.permissions(0x6001U, 1U)->rpdo_mappable);
    REQUIRE(dictionary.permissions(0x6001U, 1U)->writable);
}

TEST_CASE(od_002_writes_are_retained_across_communication_reset) {
    CanopenObjectDictionary dictionary;

    const auto heartbeat = dictionary.write(0x1017U, 0U, le(250U, 2U));
    REQUIRE_EQ(heartbeat.abort, SdoAbort::none);
    REQUIRE_EQ(heartbeat.effects.producer_heartbeat_period, 250U);
    dictionary.communication_reset();

    REQUIRE_EQ(read_value(dictionary, 0x1017U, 0U), 250U);
}

TEST_CASE(od_003_rejects_missing_read_only_and_wrong_size_access) {
    CanopenObjectDictionary dictionary;

    REQUIRE_EQ(dictionary.read(0x2222U, 0U).abort,
               SdoAbort::object_not_found);
    REQUIRE_EQ(dictionary.read(0x1000U, 1U).abort,
               SdoAbort::subindex_not_found);
    REQUIRE_EQ(dictionary.write(0x1000U, 0U, le(1U, 4U)).abort,
               SdoAbort::read_only);
    REQUIRE_EQ(dictionary.write(0x1017U, 0U, le(1U, 4U)).abort,
               SdoAbort::type_mismatch);
}

TEST_CASE(od_003_mapping_writes_validate_direction_and_exact_bit_size) {
    CanopenObjectDictionary dictionary;

    const std::uint32_t input_mapping = 0x60000120U;
    const std::uint32_t output_mapping = 0x60010120U;
    REQUIRE_EQ(dictionary.write(0x1a00U, 1U, le(input_mapping, 4U)).abort,
               SdoAbort::none);
    REQUIRE_EQ(dictionary.write(0x1600U, 1U, le(output_mapping, 4U)).abort,
               SdoAbort::none);
    REQUIRE_EQ(dictionary.write(0x1600U, 2U, le(input_mapping, 4U)).abort,
               SdoAbort::pdo_mapping);
    REQUIRE_EQ(dictionary.write(0x1a00U, 2U,
                                le(0x60000110U, 4U)).abort,
               SdoAbort::pdo_mapping);
    REQUIRE_EQ(dictionary.write(0x1600U, 0U, le(9U, 1U)).abort,
               SdoAbort::value_range);
}

TEST_CASE(od_011_error_history_clear_and_od_051_output_effects_are_reported) {
    CanopenObjectDictionary dictionary;
    dictionary.set_error_history(0U, 0x12345678U);

    REQUIRE_EQ(read_value(dictionary, 0x1003U, 0U), 1U);
    REQUIRE_EQ(read_value(dictionary, 0x1003U, 1U), 0x12345678U);
    REQUIRE_EQ(dictionary.write(0x1003U, 0U, le(0U, 1U)).abort,
               SdoAbort::none);
    REQUIRE_EQ(read_value(dictionary, 0x1003U, 0U), 0U);

    const auto output = dictionary.write(0x6001U, 1U,
                                         le(0x89abcdefU, 4U));
    REQUIRE_EQ(output.abort, SdoAbort::none);
    REQUIRE_EQ(output.effects.digital_output, 0x89abcdefU);
}

TEST_CASE(od_031_rpdo_routes_little_endian_mapping_into_dictionary) {
    CanopenObjectDictionary dictionary;
    REQUIRE_EQ(dictionary.write(0x1400U, 1U, le(0x00000200U, 4U)).abort,
               SdoAbort::none);
    REQUIRE_EQ(dictionary.write(0x1600U, 0U, le(1U, 1U)).abort,
               SdoAbort::none);
    REQUIRE_EQ(dictionary.write(0x1600U, 1U, le(0x60010120U, 4U)).abort,
               SdoAbort::none);

    CanopenReceivePdoRouter router(dictionary);
    firmware::core::CanFrame frame;
    frame.identifier = 0x200U;
    frame.size = 4U;
    frame.data = {0x78U, 0x56U, 0x34U, 0x12U, 0U, 0U, 0U, 0U};
    REQUIRE(router.receive(frame));
    REQUIRE_EQ(read_value(dictionary, 0x6001U, 1U), 0x12345678U);
}

TEST_CASE(od_041_tpdo_emits_mapped_value_after_event_timer) {
    CanopenObjectDictionary dictionary;
    REQUIRE_EQ(dictionary.write(0x1800U, 1U, le(0x00000180U, 4U)).abort,
               SdoAbort::none);
    REQUIRE_EQ(dictionary.write(0x1800U, 5U, le(20U, 2U)).abort,
               SdoAbort::none);
    REQUIRE_EQ(dictionary.write(0x1a00U, 0U, le(1U, 1U)).abort,
               SdoAbort::none);
    REQUIRE_EQ(dictionary.write(0x1a00U, 1U, le(0x60000120U, 4U)).abort,
               SdoAbort::none);

    CanopenTransmitPdoScheduler scheduler(dictionary);
    REQUIRE(!scheduler.process_cycle().has_value());
    const auto frame = scheduler.process_cycle();
    REQUIRE(frame.has_value());
    REQUIRE_EQ(frame->identifier, 0x180U);
    REQUIRE_EQ(frame->size, 4U);
    REQUIRE_EQ(frame->data[0], 0U);
    REQUIRE_EQ(frame->data[3], 0U);
}

TEST_CASE(od_040_tpdo_accepts_zero_event_driven_transmission_type) {
    CanopenObjectDictionary dictionary;
    REQUIRE_EQ(dictionary.write(0x1800U, 1U, le(0x00000180U, 4U)).abort,
               SdoAbort::none);
    REQUIRE_EQ(dictionary.write(0x1800U, 2U, le(0U, 1U)).abort,
               SdoAbort::none);
    REQUIRE_EQ(dictionary.write(0x1800U, 5U, le(10U, 2U)).abort,
               SdoAbort::none);
    REQUIRE_EQ(dictionary.write(0x1a00U, 0U, le(1U, 1U)).abort,
               SdoAbort::none);
    REQUIRE_EQ(dictionary.write(0x1a00U, 1U, le(0x60000120U, 4U)).abort,
               SdoAbort::none);

    CanopenTransmitPdoScheduler scheduler(dictionary);
    REQUIRE(scheduler.process_cycle().has_value());
}
