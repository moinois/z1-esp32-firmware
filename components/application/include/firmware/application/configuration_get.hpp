// Declares config-get source selection behind replaceable configuration I/O.
#pragma once

#include "firmware/application/live_configuration.hpp"
#include "firmware/core/frame.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace firmware::application {

// Adds fresh SD line reads and responses to the live-view loading contract.
class ConfigurationGetPort : public LiveConfigurationPort {
public:
    // Enables safe destruction through a substituted port implementation.
    ~ConfigurationGetPort() override = default;

    // Reads active configuration lines afresh for the SD source.
    virtual std::optional<std::string> read_value(std::string_view tag,
                                                  std::string_view key) = 0;

    // Sends one response to the destination selected by the command adapter.
    virtual void send(core::Frame frame) = 0;
};

// Executes cached, SD, and live configuration lookup behavior.
class ConfigurationGet {
public:
    // Parses at most two tokens and responds for one recognized source.
    static void execute(core::BytesView argument, LiveConfiguration& live,
                        ConfigurationGetPort& port);
};

}  // namespace firmware::application
