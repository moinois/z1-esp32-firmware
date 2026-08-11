// Verifies exact controller-path diagnostics required by the specification.
#include "test.hpp"

#include "application/diagnostics/controller_diagnostics.hpp"

#include <string>

TEST_CASE(diag_038_formats_controller_queue_full_type_as_two_uppercase_hex_digits) {
    REQUIRE_EQ(firmware::application::controller_queue_full_diagnostic(0x0aU),
               std::string("TxQueue full, drop frame type=0x0A"));
    REQUIRE_EQ(firmware::application::controller_queue_full_diagnostic(0xf3U),
               std::string("TxQueue full, drop frame type=0xF3"));
}
