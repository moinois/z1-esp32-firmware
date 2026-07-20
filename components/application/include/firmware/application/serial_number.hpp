// Declares persistent machine serial-number command validation and responses.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace firmware::application {

// Classifies persistent serial-number reads for exact response selection.
enum class SerialNumberReadResult {
    success,
    missing_namespace,
    missing_key,
    failure,
};

// Holds one persistent read result and its string value when available.
struct SerialNumberRead {
    SerialNumberReadResult result;
    std::string value;
};

// Isolates serial-number policy from capacity, persistence, and packet routing.
class SerialNumberPort {
public:
    // Enables safe destruction through a substituted serial-number adapter.
    virtual ~SerialNumberPort() = default;

    // Attempts admission to the shared runtime-operation capacity.
    virtual bool admit_operation(std::uint32_t wait_milliseconds) = 0;

    // Reads one persistent string from the exact namespace and key.
    virtual SerialNumberRead read_serial(std::string_view name_space,
                                         std::string_view key) = 0;

    // Writes one persistent string under the exact namespace and key.
    virtual bool write_serial(std::string_view name_space, std::string_view key,
                              std::string_view value) = 0;

    // Releases one admitted runtime-operation slot.
    virtual void complete_operation() = 0;

    // Sends one response through origin-aware runtime routing.
    virtual void send_response(std::uint8_t type,
                               std::string_view payload) = 0;
};

// Implements immutable serial-number get and set commands under bounded work.
class SerialNumberService {
public:
    // Binds serial-number rules to replaceable capacity and persistence ports.
    explicit SerialNumberService(SerialNumberPort& port);

    // Validates and executes one complete sn-get command.
    void handle_get(std::string_view command);

    // Validates and executes one complete sn-set command.
    void handle_set(std::string_view command);

private:
    // Sends one exact type-0x83 serial-number response.
    void respond(std::string_view payload);

    SerialNumberPort& port_;
};

}  // namespace firmware::application
