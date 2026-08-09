// Verifies serial-number syntax, capacity, persistence, and exact responses.
#include "test.hpp"

#include "application/runtime/serial_number.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using firmware::application::SerialNumberPort;
using firmware::application::SerialNumberRead;
using firmware::application::SerialNumberReadResult;
using firmware::application::SerialNumberService;

namespace {

// Records capacity, persistence keys, writes, completion, and packet responses.
class FakeSerialNumberPort final : public SerialNumberPort {
public:
    // Attempts runtime-operation admission with the exact wait bound.
    bool admit_operation(std::uint32_t wait_milliseconds) override {
        waits.push_back(wait_milliseconds);
        return admission_succeeds;
    }

    // Returns the configured persistent serial-number read outcome.
    SerialNumberRead read_serial(std::string_view name_space,
                                 std::string_view key) override {
        namespaces.emplace_back(name_space);
        keys.emplace_back(key);
        return read_result;
    }

    // Attempts to persist the supplied serial-number value.
    bool write_serial(std::string_view name_space, std::string_view key,
                      std::string_view value) override {
        namespaces.emplace_back(name_space);
        keys.emplace_back(key);
        writes.emplace_back(value);
        return write_succeeds;
    }

    // Releases one admitted runtime-operation slot.
    void complete_operation() override {
        ++completed_operations;
    }

    // Records one routed runtime response packet.
    void send_response(std::uint8_t type, std::string_view payload) override {
        response_types.push_back(type);
        responses.emplace_back(payload);
    }

    bool admission_succeeds = true;
    bool write_succeeds = true;
    SerialNumberRead read_result{SerialNumberReadResult::missing_namespace, {}};
    std::size_t completed_operations = 0U;
    std::vector<std::uint32_t> waits;
    std::vector<std::string> namespaces;
    std::vector<std::string> keys;
    std::vector<std::string> writes;
    std::vector<std::uint8_t> response_types;
    std::vector<std::string> responses;
};

}  // namespace

TEST_CASE(run_011_sn_get_rejects_any_non_whitespace_suffix_before_capacity) {
    FakeSerialNumberPort port;
    SerialNumberService serial(port);

    serial.handle_get("sn-get x");

    REQUIRE(port.waits.empty());
    REQUIRE_EQ(port.responses,
               std::vector<std::string>({"The command format is invalid\n"}));
}

TEST_CASE(run_011_sn_get_busy_uses_exact_wait_and_response) {
    FakeSerialNumberPort port;
    port.admission_succeeds = false;
    SerialNumberService serial(port);

    serial.handle_get("sn-get \t\r\n");

    REQUIRE_EQ(port.waits, std::vector<std::uint32_t>({200U}));
    REQUIRE_EQ(port.responses,
               std::vector<std::string>({"SN_GET_ERR: busy\n"}));
}

TEST_CASE(run_010_and_012_sn_get_maps_all_read_results_and_exact_key) {
    FakeSerialNumberPort port;
    SerialNumberService serial(port);

    serial.handle_get("sn-get");
    port.read_result = {SerialNumberReadResult::missing_key, {}};
    serial.handle_get("sn-get ");
    port.read_result = {SerialNumberReadResult::success, {}};
    serial.handle_get("sn-get\t");
    port.read_result = {SerialNumberReadResult::failure, {}};
    serial.handle_get("sn-get\n");
    port.read_result = {SerialNumberReadResult::success, "Z1-42"};
    serial.handle_get("sn-get\r");

    REQUIRE_EQ(port.responses,
               std::vector<std::string>({"sn = null\n", "sn = null\n",
                                         "sn = null\n", "sn get failed\n",
                                         "sn = Z1-42\n"}));
    REQUIRE_EQ(port.namespaces,
               std::vector<std::string>(5U, "factory"));
    REQUIRE_EQ(port.keys,
               std::vector<std::string>(5U, "machine_sn"));
    REQUIRE_EQ(port.completed_operations, 5U);
    REQUIRE_EQ(port.response_types,
               std::vector<std::uint8_t>(5U, 0x83U));
}

TEST_CASE(run_013_to_014_sn_set_validates_shape_length_and_bytes_before_busy) {
    FakeSerialNumberPort port;
    SerialNumberService serial(port);

    serial.handle_set("sn-set");
    serial.handle_set("sn-setX value");
    serial.handle_set("sn-set   ");
    serial.handle_set(std::string("sn-set ") + std::string(33U, 'x'));
    std::string illegal = "sn-set ok";
    illegal.push_back('\x7f');
    serial.handle_set(illegal);

    REQUIRE_EQ(port.responses,
               std::vector<std::string>({
                   "The command format is invalid\n",
                   "The command format is invalid\n", "Invalid len\n",
                   "Invalid len\n",
                   "There are some illegal characters in the sn\n"}));
    REQUIRE(port.waits.empty());
}

TEST_CASE(run_013_sn_set_trims_ends_but_retains_printable_internal_spaces) {
    FakeSerialNumberPort port;
    SerialNumberService serial(port);

    serial.handle_set("sn-set \t A B  \r\n");

    REQUIRE_EQ(port.writes, std::vector<std::string>({"A B"}));
    REQUIRE_EQ(port.responses,
               std::vector<std::string>({"Success:sn = A B\n"}));
    REQUIRE_EQ(port.completed_operations, 1U);
}

TEST_CASE(run_014_valid_sn_set_reports_capacity_saturation) {
    FakeSerialNumberPort port;
    port.admission_succeeds = false;
    SerialNumberService serial(port);

    serial.handle_set("sn-set value");

    REQUIRE_EQ(port.responses,
               std::vector<std::string>({"SN_SET_ERR: busy\n"}));
    REQUIRE(port.writes.empty());
}

TEST_CASE(run_015_existing_nonempty_serial_number_is_immutable) {
    FakeSerialNumberPort port;
    port.read_result = {SerialNumberReadResult::success, "existing"};
    SerialNumberService serial(port);

    serial.handle_set("sn-set replacement");

    REQUIRE(port.writes.empty());
    REQUIRE_EQ(port.responses,
               std::vector<std::string>({
                   "The value of sn cannot be changed\n"}));
    REQUIRE_EQ(port.completed_operations, 1U);
}

TEST_CASE(run_015_to_016_missing_empty_or_unreadable_value_permits_write) {
    for (const SerialNumberRead& read :
         {SerialNumberRead{SerialNumberReadResult::missing_key, {}},
          SerialNumberRead{SerialNumberReadResult::success, {}},
          SerialNumberRead{SerialNumberReadResult::failure, {}}}) {
        FakeSerialNumberPort port;
        port.read_result = read;
        port.write_succeeds = false;
        SerialNumberService serial(port);

        serial.handle_set("sn-set new");

        REQUIRE_EQ(port.writes, std::vector<std::string>({"new"}));
        REQUIRE_EQ(port.responses,
                   std::vector<std::string>({"sn set failed\n"}));
        REQUIRE_EQ(port.completed_operations, 1U);
    }
}
