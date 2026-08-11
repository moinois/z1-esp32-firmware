/** @file @brief Declares portable SoftAP command and parameter-query policy. */
#pragma once

#include "core/protocol/frame.hpp"
#include "core/protocol/text.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace firmware::application {

/** Retains mutable SoftAP values independently from persistence completion. */
struct AccessPointCommandState {
    std::optional<std::uint8_t> saved_channel;
    std::string password;
    bool enabled = true;
    std::string machine_name;
    std::string last_access_point_name;
    std::string fallback_name;
    std::uint8_t last_channel = 1U;
};

/** Describes the exact live configuration applied by `ap enable`. */
struct AccessPointRadioConfig {
    std::string ssid;
    std::string password;
    std::uint8_t channel = 1U;
};

/** Isolates SoftAP policy from NVS, configuration storage, radio, and queries. */
class AccessPointCommandPort {
public:
    virtual ~AccessPointCommandPort() = default;
    virtual bool persist_channel(std::optional<std::uint8_t> channel) = 0;
    virtual bool persist_password(std::string_view password) = 0;
    virtual bool persist_enabled(bool enabled) = 0;
    virtual bool persist_machine_name(std::string_view name) = 0;
    virtual bool enable_access_point(const AccessPointRadioConfig& config) = 0;
    virtual bool disable_access_point() = 0;
    virtual std::optional<std::string> station_parameter(
        std::uint8_t parameter) = 0;
    virtual std::optional<std::string> access_point_parameter(
        std::uint8_t parameter) = 0;
};

/** Executes APCMD and APQ commands and creates their exact addressed reply. */
class AccessPointCommandService {
public:
    static std::optional<core::Frame> execute(
        core::CommandKind kind, core::BytesView payload,
        AccessPointCommandState& state, AccessPointCommandPort& port);
};

}  // namespace firmware::application
