// Verifies USB descriptor identity, endpoint geometry, and required strings.
#include "test.hpp"

#include "firmware/application/usb_descriptors.hpp"

using firmware::application::UsbDescriptors;

TEST_CASE(usb_001_and_002_device_descriptor_has_exact_identity) {
    const auto descriptor = UsbDescriptors::device();
    REQUIRE_EQ(descriptor.size(), 18U);
    REQUIRE_EQ(descriptor[0], 0x12U);
    REQUIRE_EQ(descriptor[1], 0x01U);
    REQUIRE_EQ(descriptor[8], 0x3aU);
    REQUIRE_EQ(descriptor[9], 0x30U);
    REQUIRE_EQ(descriptor[10], 0x02U);
    REQUIRE_EQ(descriptor[11], 0x40U);
    REQUIRE_EQ(descriptor[14], 0x01U);
    REQUIRE_EQ(descriptor[15], 0x02U);
    REQUIRE_EQ(descriptor[16], 0x03U);
}

TEST_CASE(usb_001_and_003_configuration_has_vendor_interface_and_bulk_endpoints) {
    const auto descriptor = UsbDescriptors::configuration();
    REQUIRE_EQ(descriptor.size(), 32U);
    REQUIRE_EQ(descriptor[2], 0x20U);
    REQUIRE_EQ(descriptor[7], 0x80U);
    REQUIRE_EQ(descriptor[8], 0xfaU);
    REQUIRE_EQ(descriptor[14], 0xffU);
    REQUIRE_EQ(descriptor[20], 0x01U);
    REQUIRE_EQ(descriptor[27], 0x81U);
    REQUIRE_EQ(descriptor[22], 0x40U);
    REQUIRE_EQ(descriptor[29], 0x40U);
}

TEST_CASE(usb_002_string_descriptors_have_required_values) {
    REQUIRE_EQ(UsbDescriptors::string_descriptor(0U),
               firmware::core::ByteVector({0x04U, 0x03U, 0x09U, 0x04U}));
    REQUIRE_EQ(UsbDescriptors::string_descriptor(1U).size(), 20U);
    REQUIRE_EQ(UsbDescriptors::string_descriptor(2U).size(), 30U);
    REQUIRE_EQ(UsbDescriptors::string_descriptor(3U).size(), 14U);
    REQUIRE(UsbDescriptors::string_descriptor(4U).empty());
}

