// Verifies exact SoftAP mutation, query, response, and fallback policy.
#include "test.hpp"

#include "application/connectivity/access_point_command.hpp"

#include <optional>
#include <string>
#include <vector>

using firmware::application::AccessPointCommandPort;
using firmware::application::AccessPointCommandService;
using firmware::application::AccessPointCommandState;
using firmware::application::AccessPointRadioConfig;
using firmware::core::ByteVector;
using firmware::core::CommandKind;

namespace {

std::string text(const std::optional<firmware::core::Frame>& frame) {
    REQUIRE(frame.has_value());
    REQUIRE_EQ(frame->type, 0x90U);
    return std::string(frame->payload.begin(), frame->payload.end());
}

class FakeAccessPointPort final : public AccessPointCommandPort {
public:
    bool persist_channel(std::optional<std::uint8_t> channel) override {
        persisted_channel = channel;
        return channel_save_succeeds;
    }
    bool persist_password(std::string_view password) override {
        persisted_password = std::string(password);
        return password_save_succeeds;
    }
    bool persist_enabled(bool enabled) override {
        persisted_enabled = enabled;
        return enable_save_succeeds;
    }
    bool persist_machine_name(std::string_view name) override {
        persisted_machine_name = std::string(name);
        return name_save_succeeds;
    }
    bool enable_access_point(const AccessPointRadioConfig& config) override {
        enabled_config = config;
        return radio_succeeds;
    }
    bool disable_access_point() override {
        disable_called = true;
        return radio_succeeds;
    }
    std::optional<std::string> station_parameter(std::uint8_t parameter) override {
        return station_values[parameter];
    }
    std::optional<std::string> access_point_parameter(
        std::uint8_t parameter) override {
        return access_point_values[parameter];
    }
    void report_query_failure(bool station, std::uint8_t parameter) override {
        query_failures.push_back({station, parameter});
    }

    bool channel_save_succeeds = true;
    bool password_save_succeeds = true;
    bool enable_save_succeeds = true;
    bool name_save_succeeds = true;
    bool radio_succeeds = true;
    bool disable_called = false;
    std::optional<std::uint8_t> persisted_channel;
    std::optional<bool> persisted_enabled;
    std::optional<std::string> persisted_password;
    std::optional<std::string> persisted_machine_name;
    std::optional<AccessPointRadioConfig> enabled_config;
    std::optional<std::string> station_values[8];
    std::optional<std::string> access_point_values[8];
    std::vector<std::pair<bool, std::uint8_t>> query_failures;
};

std::optional<firmware::core::Frame> execute(
    AccessPointCommandState& state, FakeAccessPointPort& port,
    std::string_view command, CommandKind kind = CommandKind::access_point) {
    return AccessPointCommandService::execute(
        kind,
        ByteVector(command.begin(), command.end()),
        state,
        port);
}

}  // namespace

TEST_CASE(apcmd_001_invalid_shapes_return_exact_error) {
    AccessPointCommandState state;
    FakeAccessPointPort port;
    for (const std::string_view command : {"ap", "ap\tget", "ap  get", "ap GET"}) {
        REQUIRE_EQ(text(execute(state, port, command)),
                   std::string("ERROR: Invalid AP Command!\r\n"));
    }
    REQUIRE_EQ(text(execute(state, port, "ap enable\t")),
               std::string("ERROR: Invalid AP Command!\r\n"));
    REQUIRE_EQ(text(execute(state, port, "ap enable \r\n")),
               std::string("AP enabled\r\n"));
}

TEST_CASE(apcmd_002_get_uses_name_password_and_channel_fallbacks) {
    AccessPointCommandState state;
    state.fallback_name = "Makera_Z1_ABCD";
    state.last_access_point_name = "retained";
    state.last_channel = 14U;
    FakeAccessPointPort port;

    REQUIRE_EQ(text(execute(state, port, "ap get ignored")),
               std::string("AP enable=1 ssid=retained password=null channel=14\r\n"));

    state.machine_name = "configured";
    state.password = "password8";
    state.saved_channel = 7U;
    state.enabled = false;
    REQUIRE_EQ(text(execute(state, port, "ap get")),
               std::string("AP enable=0 ssid=configured password=password8 channel=7\r\n"));
}

TEST_CASE(apcmd_003_channel_uses_initial_decimal_clear_and_state_before_save) {
    AccessPointCommandState state;
    FakeAccessPointPort port;

    REQUIRE_EQ(text(execute(state, port, "ap channel 6suffix later")),
               std::string("AP channel saved, apply on reboot\r\n"));
    REQUIRE_EQ(state.saved_channel, std::optional<std::uint8_t>(6U));
    REQUIRE_EQ(text(execute(state, port, "ap channel   +7suffix later")),
               std::string("AP channel saved, apply on reboot\r\n"));
    REQUIRE_EQ(state.saved_channel, std::optional<std::uint8_t>(7U));
    REQUIRE_EQ(text(execute(state, port, "ap channel -1")),
               std::string("WiFi AP Channel should between 1 to 11\r\n"));
    REQUIRE_EQ(text(execute(state, port, "ap channel 999999999999999999999")),
               std::string("WiFi AP Channel should between 1 to 11\r\n"));

    port.channel_save_succeeds = false;
    REQUIRE_EQ(text(execute(state, port, "ap channel 9")),
               std::string("Failed to save AP channel\r\n"));
    REQUIRE_EQ(state.saved_channel, std::optional<std::uint8_t>(9U));

    REQUIRE_EQ(text(execute(state, port, "ap channel clear")),
               std::string("Failed to clear AP channel\r\n"));
    REQUIRE(!state.saved_channel.has_value());
    REQUIRE_EQ(text(execute(state, port, "ap channel 12")),
               std::string("WiFi AP Channel should between 1 to 11\r\n"));
}

TEST_CASE(apcmd_004_ssid_uses_remainder_escape_and_exact_length) {
    AccessPointCommandState state;
    FakeAccessPointPort port;
    std::string command = "ap ssid   My";
    command.push_back(0x01);
    command += "Machine  \t";

    REQUIRE_EQ(text(execute(state, port, command)),
               std::string("AP ssid saved, apply on reboot\r\n"));
    REQUIRE_EQ(state.machine_name, std::string("My Machine  \t"));
    REQUIRE_EQ(port.persisted_machine_name,
               std::optional<std::string>("My Machine  \t"));

    port.name_save_succeeds = false;
    std::string nul_terminated = "ap ssid Runtime";
    nul_terminated.push_back('\0');
    nul_terminated += "ignored";
    REQUIRE_EQ(text(execute(state, port, nul_terminated)),
               std::string("Failed to save AP ssid\r\n"));
    REQUIRE_EQ(state.machine_name, std::string("Runtime"));

    REQUIRE_EQ(text(execute(state, port, "ap ssid ")),
               std::string("WiFi AP SSID length should between 1 to 23\r\n"));
    REQUIRE_EQ(text(execute(state, port, "ap ssid 123456789012345678901234")),
               std::string("WiFi AP SSID length should between 1 to 23\r\n"));
}

TEST_CASE(apcmd_005_password_uses_first_token_and_exact_clear_form) {
    AccessPointCommandState state;
    FakeAccessPointPort port;

    REQUIRE_EQ(text(execute(state, port, "ap password abcdefgh ignored")),
               std::string("AP password saved, apply on reboot\r\n"));
    REQUIRE_EQ(state.password, std::string("abcdefgh"));

    port.password_save_succeeds = false;
    REQUIRE_EQ(text(execute(state, port, "ap password runtime8")),
               std::string("Failed to save AP password\r\n"));
    REQUIRE_EQ(state.password, std::string("runtime8"));
    port.password_save_succeeds = true;
    REQUIRE_EQ(text(execute(state, port, "ap password clear")),
               std::string("AP password cleared, open network on reboot\r\n"));
    REQUIRE(state.password.empty());
    REQUIRE_EQ(text(execute(state, port, "ap password clear later")),
               std::string("WiFi AP password length should between 8 to 63\r\n"));
}

TEST_CASE(apcmd_006_enable_and_disable_persist_before_radio_without_rollback) {
    AccessPointCommandState state;
    state.machine_name = "machine";
    state.password = "password8";
    state.last_channel = 14U;
    FakeAccessPointPort port;

    REQUIRE_EQ(text(execute(state, port, "ap enable ignored")),
               std::string("AP enabled\r\n"));
    REQUIRE(state.enabled);
    REQUIRE(port.enabled_config.has_value());
    REQUIRE_EQ(port.enabled_config->channel, 1U);
    REQUIRE_EQ(port.enabled_config->ssid, std::string("machine"));

    port.radio_succeeds = false;
    REQUIRE_EQ(text(execute(state, port, "ap disable")),
               std::string("Failed to disable AP\r\n"));
    REQUIRE(!state.enabled);

    port.enable_save_succeeds = false;
    REQUIRE_EQ(text(execute(state, port, "ap enable")),
               std::string("Failed to save AP enable\r\n"));
    REQUIRE(state.enabled);
}

TEST_CASE(apq_001_to_003_query_shapes_values_and_errors_are_exact) {
    AccessPointCommandState state;
    FakeAccessPointPort port;
    port.station_values[0] = "Away";
    port.access_point_values[7] = "2";

    REQUIRE_EQ(text(execute(state, port, "M482", CommandKind::station_parameter_query)),
               std::string("M482 param[0]:Away\n"));
    REQUIRE_EQ(text(execute(state, port, "M483.7", CommandKind::access_point_parameter_query)),
               std::string("M483 param[7]:2\n"));
    port.station_values[1] = "";
    REQUIRE_EQ(text(execute(state, port, "M482.1", CommandKind::station_parameter_query)),
               std::string("M482 param[1]:null\n"));
    REQUIRE_EQ(text(execute(state, port, "M482.8", CommandKind::station_parameter_query)),
               std::string("Query WiFi STA parameters ERROR!\n"));
    REQUIRE_EQ(text(execute(state, port, "M483.0x", CommandKind::access_point_parameter_query)),
               std::string("Query WiFi AP parameters ERROR!\n"));
    REQUIRE_EQ(text(execute(state, port, "M482.2", CommandKind::station_parameter_query)),
               std::string("Query WiFi STA parameters ERROR!\n"));
    const std::vector<std::pair<bool, std::uint8_t>> expected_failures{{true, 2U}};
    REQUIRE_EQ(port.query_failures, expected_failures);
    REQUIRE_EQ(text(execute(state, port, "M482.1\t\r\n",
                            CommandKind::station_parameter_query)),
               std::string("M482 param[1]:null\n"));
    REQUIRE_EQ(text(execute(state, port, "M482.1\v",
                            CommandKind::station_parameter_query)),
               std::string("Query WiFi STA parameters ERROR!\n"));
    REQUIRE_EQ(port.query_failures.size(), 1U);
}
