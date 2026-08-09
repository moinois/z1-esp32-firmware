/** @file @brief Declares live and SD config-set policy behind replaceable persistence I/O. */
#pragma once

#include "application/configuration/live_configuration.hpp"
#include "core/protocol/frame.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace firmware::application {

/// Adds SD replacement operations and responses to live-view loading.
class ConfigurationSetPort : public LiveConfigurationPort {
public:
    /// Enables safe destruction through a substituted port implementation.
    ~ConfigurationSetPort() override = default;

    /// Reads the complete active configuration or reports source failure.
    virtual bool set_value(std::string_view tag, std::string_view key,
                           std::string_view value) = 0;

    /// Sends one response to the destination selected by the command adapter.
    virtual void send(core::Frame frame) = 0;
};

/// Executes bounded live updates and temporary-file SD replacement.
class ConfigurationSet {
public:
    /// Parses at most three tokens and applies one recognized source update.
    static void execute(core::BytesView argument, LiveConfiguration& live,
                        ConfigurationSetPort& port);
};

}  // namespace firmware::application
