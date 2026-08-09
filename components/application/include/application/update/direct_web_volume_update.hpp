/** @file @brief Declares direct web-volume erase/write/restart orchestration. */
#pragma once

#include "core/protocol/bytes.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace firmware::application {

/// Provides partition capacity, erase, write, response, and restart effects.
class DirectWebVolumeUpdatePort {
public:
    /// Enables safe destruction through a substituted partition adapter.
    virtual ~DirectWebVolumeUpdatePort() = default;

    /// Returns the complete web-volume partition capacity.
    virtual std::optional<std::size_t> partition_size() = 0;

    /// Erases the complete partition before writing offset zero.
    virtual bool erase_partition() = 0;

    /// Writes all extracted bytes at partition offset zero.
    virtual bool write_content(core::BytesView content) = 0;

    /// Sends one complete HTTP result body.
    virtual void send_response(std::uint16_t status, std::string_view body) = 0;

    /// Waits before the requested restart.
    virtual void delay_milliseconds(std::uint32_t milliseconds) = 0;

    /// Restarts the mainboard after successful replacement.
    virtual void restart() = 0;
};

/// Applies one raw web-volume image without filesystem-image validation.
class DirectWebVolumeUpdateService {
public:
    /// Executes erase, bounded write, success response, delay, and restart.
    bool apply(core::BytesView content, DirectWebVolumeUpdatePort& port) const;

    /// Convenience overload for owned host-side test images.
    bool apply(const core::ByteVector& content,
               DirectWebVolumeUpdatePort& port) const {
        return apply(core::BytesView(content), port);
    }

private:
    /// Publishes one HTTP 500 response and returns failure.
    static bool fail(DirectWebVolumeUpdatePort& port, std::string_view body);
};

}  // namespace firmware::application
