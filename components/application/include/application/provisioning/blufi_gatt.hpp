/** @file @brief Declares transport-independent BLUFI GATT transaction and retry policy. */
#pragma once

#include "core/protocol/bytes.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace firmware::application {

/// Names the ATT status values selected by deterministic prepared-write cases.
enum class BlufiAttStatus : std::uint8_t {
    success = 0x00U,
    invalid_offset = 0x07U,
    invalid_length = 0x0DU,
    allocation_failure = 0x80U,
};

/// Holds the fields echoed by one prepared-write ATT response.
struct BlufiPreparedWriteResponse {
    BlufiAttStatus status;
    std::uint16_t handle;
    std::size_t offset;
    core::ByteVector value;
};

/// Isolates GATT policy from BLE callbacks, allocation, decoding, and timing.
class BlufiGattPort {
public:
    /// Enables safe destruction through a substituted GATT adapter.
    virtual ~BlufiGattPort() = default;

    /// Allocates the exact zeroed shared prepared-write buffer.
    virtual std::optional<core::ByteVector> allocate_prepared(
        std::size_t size) = 0;

    /// Sends an ATT response for a normal write when one was requested.
    virtual void send_write_response(BlufiAttStatus status) = 0;

    /// Passes one complete characteristic value to BLUFI frame decoding.
    virtual void decode_frame(core::BytesView frame) = 0;

    /// Attempts one notification without waiting for capacity.
    virtual bool send_notification(core::BytesView frame) = 0;

    /// Reports whether the connection required for notification still exists.
    virtual bool connected() const = 0;

    /// Delays the next notification attempt by the requested duration.
    virtual void delay_milliseconds(std::uint32_t duration) = 0;
};

/// Owns the shared prepared aggregate and applies exact GATT transaction order.
class BlufiGattPolicy {
public:
    /// Creates an empty per-connection GATT policy using the supplied adapter.
    explicit BlufiGattPolicy(BlufiGattPort& port);

    /// Discards prepared state when a BLE connection starts or ends.
    void reset();

    /// Returns the fixed readable outgoing-characteristic value.
    core::ByteVector read_outgoing_characteristic() const;

    /// Returns the fixed readable client-configuration descriptor value.
    core::ByteVector read_client_configuration() const;

    /// Responds to a normal control write before passing it to BLUFI decoding.
    void write_control(core::BytesView value, bool response_requested);

    /// Accepts a descriptor write without retaining or applying its value.
    void write_client_configuration(core::BytesView value,
                                    bool response_requested);

    /// Accepts and echoes one prepared chunk or returns its exact ATT error.
    BlufiPreparedWriteResponse prepare_write(std::uint16_t handle,
                                             std::size_t offset,
                                             core::BytesView value);

    /// Executes or cancels the current aggregate, then always discards it.
    BlufiAttStatus finish_prepared(bool execute);

    /// Retries notifications every 10 ms while the connection remains present.
    bool notify(core::BytesView frame);

private:
    /// Discards the complete prepared-write aggregate and recorded length.
    void discard_prepared();

    /// Builds an error response after discarding the complete aggregate.
    BlufiPreparedWriteResponse reject_prepared(BlufiAttStatus status,
                                               std::uint16_t handle,
                                               std::size_t offset);

    BlufiGattPort& port_;
    std::optional<core::ByteVector> prepared_;
    std::size_t recorded_length_ = 0U;
};

}  // namespace firmware::application
