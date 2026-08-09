/** @file @brief Transport-neutral ownership for transfers and playback. */
#pragma once

#include <cstdint>
#include <optional>

namespace firmware::application {

/// Exact host response when another logical client owns file transfer state.
inline constexpr char file_owner_limit_message[] =
    "Other client is currently uploading/downloading files. Please try again later.";

/** Host transport participating in shared ownership arbitration. */
enum class HostTransport {
    tcp,
    usb
};

/** Stable connection identity, including slot reuse generation. */
struct HostIdentity {
    /// Physical host transport.
    HostTransport transport = HostTransport::tcp;
    /// Bounded connection slot; USB conventionally uses its fixed slot.
    std::uint8_t slot = 0;
    /// Increments when a physical TCP slot is reused by a new connection.
    std::uint32_t generation = 0;

    /// Compares physical identities, including slot generation.
    bool operator==(const HostIdentity& other) const {
        return transport == other.transport && slot == other.slot && generation == other.generation;
    }
};

/** Arbitrates the deliberately different file and playback owner lifetimes. */
class Ownership {
public:
    /// Claims file ownership, allowing a repeat from the same logical owner.
    bool claim_file(const HostIdentity& host);

    /// Reports whether a host has the logical identity of the file owner.
    bool is_file_owner(const HostIdentity& host) const;

    /// Releases file ownership after a protocol terminal event.
    void release_file();

    /// Reports whether any file owner exists.
    bool has_file_owner() const;

    /// Claims playback, allowing a repeat only from the same physical connection.
    bool claim_play(const HostIdentity& host);

    /// Reports whether a physical connection owns streamed playback.
    bool is_play_owner(const HostIdentity& host) const;

    /// Releases playback ownership after cleanup.
    void release_play();

    /// Reports whether any playback owner exists.
    bool has_play_owner() const;

    /** Applies distinct disconnect rules: TCP file ownership survives slot
     *  reconnect by logical identity, while playback ownership does not.
     */
    void transport_disconnected(const HostIdentity& host);

private:
    struct FileOwner {
        HostTransport transport;
        std::uint8_t slot;
    };

    std::optional<FileOwner> file_owner_;
    std::optional<HostIdentity> play_owner_;
};

}  // namespace firmware::application
