// Defines transport-neutral ownership for file transfer and streamed playback.
#pragma once

#include <cstdint>
#include <optional>

namespace firmware::application {

inline constexpr char file_owner_limit_message[] =
    "Other client is currently uploading/downloading files. Please try again later.";

enum class HostTransport {
    tcp,
    usb
};

struct HostIdentity {
    HostTransport transport = HostTransport::tcp;
    std::uint8_t slot = 0;
    std::uint32_t generation = 0;

    // Compares physical connection identities, including connection generation.
    bool operator==(const HostIdentity& other) const {
        return transport == other.transport && slot == other.slot && generation == other.generation;
    }
};

class Ownership {
public:
    // Claims file-transfer ownership, allowing a repeat from the logical owner.
    bool claim_file(const HostIdentity& host);

    // Reports whether a host has the logical identity of the file owner.
    bool is_file_owner(const HostIdentity& host) const;

    // Releases file-transfer ownership after a protocol terminal event.
    void release_file();

    // Reports whether any file-transfer owner exists.
    bool has_file_owner() const;

    // Claims play ownership, allowing a repeat only from the same connection.
    bool claim_play(const HostIdentity& host);

    // Reports whether a physical connection owns streamed playback.
    bool is_play_owner(const HostIdentity& host) const;

    // Releases play ownership after playback cleanup.
    void release_play();

    // Reports whether any play owner exists.
    bool has_play_owner() const;

    // Applies the different file and play rules for a transport disconnect.
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
