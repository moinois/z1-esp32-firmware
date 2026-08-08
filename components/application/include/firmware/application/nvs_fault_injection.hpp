// Declares deterministic failure selection at persistent NVS boundaries.
#pragma once

#include <atomic>
#include <cstdint>

namespace firmware::application {

enum class NvsFault : std::uint8_t {
    none,
    open,
    commit,
};

// Stores one active NVS failure without replacing the persistent backend.
class NvsFaultInjection {
public:
    // Replaces the active failure; selecting none restores normal operation.
    void select(NvsFault fault);

    // Returns true when namespace opening must fail before calling ESP-IDF.
    bool fail_open() const;

    // Returns true when a write must fail before committing it to flash.
    bool fail_commit() const;

private:
    std::atomic<NvsFault> fault_{NvsFault::none};
};

}  // namespace firmware::application
