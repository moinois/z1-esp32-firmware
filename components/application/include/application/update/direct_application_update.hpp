/** @file @brief Declares direct application OTA orchestration behind replaceable target calls. */
#pragma once

#include "core/protocol/bytes.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace firmware::application {

/// Provides camera, partition, OTA, response, and restart side effects.
class DirectApplicationUpdatePort {
public:
    /// Enables safe destruction through a substituted update adapter.
    virtual ~DirectApplicationUpdatePort() = default;

    /// Requests camera deinitialization before partition operations.
    virtual bool deinitialize_camera() = 0;

    /// Selects the inactive application partition.
    virtual bool select_inactive_partition() = 0;

    /// Erases the complete selected application partition.
    virtual bool erase_partition() = 0;

    /// Starts an OTA write for the complete extracted image size.
    virtual bool begin_update(std::size_t size) = 0;

    /// Writes all extracted image bytes.
    virtual bool write_image(core::BytesView image) = 0;

    /// Finishes and structurally validates the active OTA image.
    virtual bool finish_update() = 0;

    /// Selects the finalized image for the next boot.
    virtual bool select_boot_partition() = 0;

    /// Aborts an active OTA write after a write/finalization failure.
    virtual void abort_update() = 0;

    /// Sends one complete HTTP result body.
    virtual void send_response(std::uint16_t status, std::string_view body) = 0;

    /// Waits without blocking the portable policy's caller contract.
    virtual void delay_milliseconds(std::uint32_t milliseconds) = 0;

    /// Restarts the mainboard after a successful handoff.
    virtual void restart() = 0;
};

/// Applies one direct image with exact failure ordering and success handoff.
class DirectApplicationUpdateService {
public:
    /// Initializes the OTA destination before multipart metadata is inspected.
    bool begin(DirectApplicationUpdatePort& port) const;

    /// Offers one extracted receive block; write failure is deferred to finish.
    void offer(core::BytesView block, DirectApplicationUpdatePort& port) const;

    /// Finalizes, selects, responds, and restarts after input has ended.
    bool finish(bool content_received, DirectApplicationUpdatePort& port) const;

    /// Runs the full direct application update transaction.
    bool apply(core::BytesView image, DirectApplicationUpdatePort& port) const;

    /// Convenience overload for owned host-side test images.
    bool apply(const core::ByteVector& image,
               DirectApplicationUpdatePort& port) const {
        return apply(core::BytesView(image), port);
    }

private:
    /// Publishes one HTML 500 response and returns failure.
    static bool fail(DirectApplicationUpdatePort& port, std::string_view body);
};

}  // namespace firmware::application
