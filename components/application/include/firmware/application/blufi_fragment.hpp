// Declares BLUFI outgoing fragmentation and per-connection input reassembly.
#pragma once

#include "firmware/application/blufi_wire.hpp"
#include "firmware/core/bytes.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace firmware::application {

// Isolates fragment policy from allocation, framing, and error transport.
class BlufiFragmentPort {
public:
    // Enables safe destruction through a substituted fragment adapter.
    virtual ~BlufiFragmentPort() = default;

    // Sends one data frame through the BLUFI wire layer.
    virtual bool send_data(std::uint8_t subtype, core::BytesView data,
                           bool non_final) = 0;

    // Allocates one exact zeroed buffer for a fragmented logical message.
    virtual std::optional<core::ByteVector> allocate_message(
        std::size_t size) = 0;

    // Reports one exact BLUFI protocol error value.
    virtual void report_error(std::uint8_t error) = 0;
};

// Splits outgoing data and reassembles one retained incoming logical message.
class BlufiFragmentSession {
public:
    // Creates a connection session with the default 23-byte ATT MTU.
    explicit BlufiFragmentSession(BlufiFragmentPort& port);

    // Discards all partial input when a BLE connection starts or ends.
    void reset();

    // Selects the negotiated ATT MTU used for later outgoing fragmentation.
    void set_att_mtu(std::uint16_t mtu);

    // Sends an ordinary or fragmented logical data message.
    bool send_data(std::uint8_t subtype, core::BytesView message);

    // Returns an ordinary or completely reassembled incoming message.
    std::optional<BlufiIncomingFrame> receive(BlufiIncomingFrame frame);

private:
    // Returns the content bytes available in each non-final fragment.
    std::size_t fragment_content_capacity() const;

    // Reports whether a frame belongs to the retained partial message.
    bool matches_partial(const BlufiIncomingFrame& frame) const;

    // Discards all metadata and storage for the retained partial message.
    void clear_partial();

    BlufiFragmentPort& port_;
    std::uint16_t att_mtu_;
    std::optional<core::ByteVector> partial_message_;
    std::size_t accumulated_size_ = 0U;
    BlufiFrameType partial_type_ = BlufiFrameType::control;
    std::uint8_t partial_subtype_ = 0U;
    bool fragmented_input_active_ = false;
};

}  // namespace firmware::application
