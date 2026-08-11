/** @file @brief Declares the shared target bridge for USB/TCP SoftAP commands. */
#pragma once

#include "application/connectivity/access_point_command.hpp"
#include "application/connectivity/connectivity_startup.hpp"

#include <optional>
#include <string_view>

namespace firmware::target {

/** Initializes retained SoftAP command state after connectivity startup. */
void initialize_access_point_commands(
    std::string_view machine_name,
    const firmware::application::AccessPointStartupSettings& settings);

/** Executes one SoftAP command while serializing shared USB/TCP state. */
std::optional<firmware::core::Frame> execute_access_point_command(
    firmware::core::CommandKind kind, firmware::core::BytesView payload);

}  // namespace firmware::target
