// Declares bounded firmware-update file deletion and permission recovery.
#pragma once

#include <cstdint>
#include <string_view>

namespace firmware::application {

// Classifies the unlink outcomes that select normative recovery behavior.
enum class UpdateDeleteResult {
    success,
    busy,
    permission_denied,
    read_only_filesystem,
    other_failure,
};

// Isolates update deletion policy from FAT, POSIX, timing, and host transport.
class UpdateDeletionPort {
public:
    // Enables safe destruction through a substituted deletion adapter.
    virtual ~UpdateDeletionPort() = default;

    // Attempts to unlink one exact update path.
    virtual UpdateDeleteResult unlink_file(std::string_view path) = 0;

    // Clears FAT read-only, hidden, and archive attributes.
    virtual bool clear_fat_attributes(std::string_view path) = 0;

    // Applies a POSIX-compatible fallback mode to the file.
    virtual bool set_mode(std::string_view path, std::uint32_t mode) = 0;

    // Delays a recovery or retry by the exact requested duration.
    virtual void delay_milliseconds(std::uint32_t duration) = 0;

    // Broadcasts one update result to all host connections.
    virtual void broadcast(std::uint8_t type, std::string_view payload) = 0;
};

// Performs at most three update-file deletion attempts under exact recovery.
class UpdateDeletionService {
public:
    // Binds the deterministic policy to replaceable filesystem and host ports.
    explicit UpdateDeletionService(UpdateDeletionPort& port);

    // Deletes one update file or broadcasts its final manual-removal error.
    bool remove(std::string_view path);

private:
    // Attempts permission recovery selected by the unlink failure class.
    bool recover_permissions(std::string_view path,
                             UpdateDeleteResult failure);

    // Sends the exact unrate-limited final deletion failure broadcast.
    void broadcast_failure(std::string_view path);

    UpdateDeletionPort& port_;
};

}  // namespace firmware::application
