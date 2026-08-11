/** @file
 *  @brief Common wire-frame encoding and transport-specific stream recovery.
 */
#pragma once
#include "core/protocol/bytes.hpp"
#include "core/protocol/protocol_constants.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
namespace firmware::core {
/** A decoded protocol packet independent of its transport framing. */
struct Frame {
    /// Wire packet identifier used to select the command or response family.
    std::uint8_t type = 0;
    /// Application bytes excluding framing, length, CRC, and tail markers.
    ByteVector payload;
    /// Compares the complete application-visible packet representation.
    bool operator==(const Frame& other) const {
        return type == other.type && payload == other.payload;
    }
};
/** Controls how an incremental decoder recovers from a malformed candidate. */
enum class RecoveryMode {
    /// Resume scanning within buffered bytes so a nested header can be found.
    scan_inside_candidate,
    /// Drop the entire candidate; required when the transport supplies blocks.
    discard_candidate
};

/// Tracks UART rejection work and requests the periodic extra-byte discard.
class UartCandidateCheckBudget {
public:
    /// Maximum raw bytes retained when decoder servicing falls behind.
    static constexpr std::size_t undecoded_capacity = 2048U;
    /// Oldest bytes discarded each time receive retention reaches capacity.
    static constexpr std::size_t overflow_discard_size = 256U;
    static constexpr std::size_t unsuccessful_check_limit = 5000U;

    /// Returns true after each complete group of 5000 unsuccessful checks.
    bool rejected_candidate();

    /// Starts a new consecutive sequence after a frame or incomplete candidate.
    void reset();

private:
    std::size_t unsuccessful_checks_ = 0U;
};

/** Transport-dependent limits and recovery rules for one decoder instance. */
struct StreamPolicy {
    /// Largest complete encoded frame accepted from this transport.
    std::size_t maximum_frame_size;
    /// Strategy applied after length, CRC, or tail validation fails.
    RecoveryMode recovery;
    /// Enables the alternate CRC for controller update packet families.
    bool controller_update_crc;
    /// Enables the USB candidate deadlines defined independently of framing.
    bool timed_usb_candidates;

    /// Returns the UART policy, including nested-header recovery and update CRC.
    static constexpr StreamPolicy controller_uart() {
        return {protocol::controller_maximum_frame_size,
                RecoveryMode::scan_inside_candidate, true, false};
    }

    /// Returns the TCP byte-stream policy.
    static constexpr StreamPolicy tcp() {
        return {protocol::host_maximum_frame_size,
                RecoveryMode::scan_inside_candidate, false, false};
    }

    /// Returns the USB block-oriented recovery policy.
    static constexpr StreamPolicy usb() {
        return {protocol::host_maximum_frame_size,
                RecoveryMode::discard_candidate, false, true};
    }
};
/** Encodes a packet with the common host-facing CRC.
 *  @param frame Application packet to encode.
 *  @return Complete wire frame ready for a host transport.
 */
ByteVector encode_frame(const Frame& frame);

/** Encodes a controller-bound packet, selecting the update CRC only for the
 *  controller firmware family mandated by FRM-007.
 *  @param frame Application packet to encode.
 *  @return Complete wire frame ready for the controller UART.
 */
ByteVector encode_controller_frame(const Frame& frame);

/** Incrementally reconstructs frames while retaining an incomplete suffix. */
class StreamDecoder {
public:
    /** Creates a decoder with immutable transport-specific behavior.
     *  @param policy Size, recovery, and CRC rules for the input stream.
     */
    explicit StreamDecoder(StreamPolicy policy) : policy_(policy) {}

    /** Appends bytes and returns every complete valid frame now available.
     *  Invalid candidates are handled according to the configured recovery
     *  mode; an incomplete final candidate remains buffered for the next call.
     *  @param input Newly received bytes; may be empty or arbitrarily split.
     *  @return Valid frames in wire order.
     */
    std::vector<Frame> push(BytesView input);

    /** Appends bytes using a monotonic timestamp for USB candidate expiry.
     *  @param input Newly received bytes.
     *  @param now_milliseconds Monotonic receive time for the complete block.
     */
    std::vector<Frame> push(BytesView input, std::uint64_t now_milliseconds);

    /// Forgets buffered partial input, for example after transport disconnect.
    void reset() {
        buffer_.clear();
        header_started_milliseconds_.reset();
        last_body_byte_milliseconds_.reset();
    }
private:
    std::vector<Frame> push_internal(
        BytesView input, std::optional<std::uint64_t> now_milliseconds);

    /// Immutable decoding rules chosen for the owning transport.
    StreamPolicy policy_;
    /// Unconsumed stream suffix beginning at a possible frame header.
    ByteVector buffer_;
    std::optional<std::uint64_t> header_started_milliseconds_;
    std::optional<std::uint64_t> last_body_byte_milliseconds_;
    UartCandidateCheckBudget uart_candidate_budget_;
};
}  // namespace firmware::core
