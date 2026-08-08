/** @file @brief Declares static-file serving through replaceable storage and HTTP ports. */
#pragma once

#include "firmware/core/bytes.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace firmware::application {

/// Provides bounded file operations for one static web-volume resource.
class StaticFilePort {
public:
    /// Enables safe destruction through a substituted filesystem adapter.
    virtual ~StaticFilePort() = default;

    /// Opens a path and reports its size, or reports a missing/unopenable file.
    virtual std::optional<std::uint64_t> open(std::string_view path) = 0;

    /// Reads at most the requested bytes from the currently open file.
    virtual std::optional<core::ByteVector> read(std::size_t maximum_bytes) = 0;

    /// Closes the currently open file.
    virtual void close() = 0;
};

/// Sends either an HTML error or chunked static-file response.
class StaticFileResponsePort {
public:
    /// Enables safe destruction through a substituted HTTP adapter.
    virtual ~StaticFileResponsePort() = default;

    /// Sends one complete non-streaming error response.
    virtual void send_error(std::uint16_t status, std::string_view content_type,
                            std::string_view body) = 0;

    /// Starts a chunked response with the selected MIME type.
    virtual void begin_chunked(std::string_view content_type) = 0;

    /// Sends one non-empty file chunk.
    virtual void send_chunk(core::BytesView chunk) = 0;

    /// Sends the terminating empty HTTP chunk.
    virtual void finish_chunks() = 0;
};

/// Resolves and streams one complete static request URI.
class StaticFileServer {
public:
    /// Returns exact 404 text for invalid/missing paths and streams existing files.
    void serve(std::string_view request_uri, StaticFilePort& file,
               StaticFileResponsePort& response) const;
};

}  // namespace firmware::application
