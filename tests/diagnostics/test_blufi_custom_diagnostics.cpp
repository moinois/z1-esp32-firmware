// Verifies the exact DIAG-025 BLUFI custom-data diagnostic records.
#include "test.hpp"

#include "application/diagnostics/blufi_custom_diagnostics.hpp"

#include <string>
using firmware::application::blufi_custom_data_hex_message;
using firmware::application::blufi_custom_data_length_message;
using firmware::core::ByteVector;

TEST_CASE(diag_025_empty_custom_data_emits_only_its_length_record) {
    REQUIRE_EQ(blufi_custom_data_length_message(0U),
               std::string("Recv custom data, len=0"));
    REQUIRE(!blufi_custom_data_hex_message({}, 0U).has_value());
}

TEST_CASE(diag_025_custom_data_uses_exact_lowercase_sixteen_byte_groups) {
    const ByteVector data{0x00U, 0x09U, 0x0aU, 0x10U, 0x7fU, 0x80U, 0xfeU,
                          0xffU, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 0xabU};
    REQUIRE_EQ(blufi_custom_data_length_message(data.size()),
               std::string("Recv custom data, len=17"));
    REQUIRE_EQ(*blufi_custom_data_hex_message(data, 0U),
               std::string("00 09 0a 10 7f 80 fe ff 01 02 03 04 05 06 07 08"));
    REQUIRE_EQ(*blufi_custom_data_hex_message(data, 16U), std::string("ab"));
    REQUIRE(!blufi_custom_data_hex_message(data, 17U).has_value());
}

TEST_CASE(diag_025_custom_data_logs_at_most_the_first_65535_bytes) {
    ByteVector data(65536U, 0x5aU);
    data[65534U] = 0x11U;
    data[65535U] = 0x22U;

    REQUIRE_EQ(blufi_custom_data_length_message(data.size()),
               std::string("Recv custom data, len=65536"));
    REQUIRE_EQ(*blufi_custom_data_hex_message(data, 65520U),
               std::string("5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 11"));
    REQUIRE(!blufi_custom_data_hex_message(data, 65535U).has_value());
}
