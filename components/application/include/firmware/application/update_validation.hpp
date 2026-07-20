// Declares aggregate update loading, validation, and exact failure effects.
#pragma once

#include "firmware/core/bytes.hpp"
#include "firmware/core/update_package.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace firmware::application {

// Distinguishes absent, nondestructive, and destructive aggregate read results.
enum class UpdateLoadFailure {
    none,
    absent,
    seek,
    size,
    allocation,
    short_read,
};

// Holds a complete aggregate or the exact stage at which loading failed.
struct UpdateLoadResult {
    UpdateLoadFailure failure;
    core::ByteVector bytes;
};

// Holds one fully validated aggregate and its decoded metadata.
struct ValidatedUpdatePackage {
    core::UpdateHeader header;
    core::ByteVector bytes;
};

// Isolates validation policy from storage, phase state, hosts, and ESP images.
class UpdateValidationPort {
public:
    // Enables safe destruction through a substituted validation adapter.
    virtual ~UpdateValidationPort() = default;

    // Best-effort removes the exact partial aggregate path.
    virtual void remove_partial(std::string_view path) = 0;

    // Best-effort clears FAT attributes before opening the aggregate.
    virtual void clear_attributes(std::string_view path) = 0;

    // Loads the complete aggregate or classifies its failure stage.
    virtual UpdateLoadResult load_aggregate(std::string_view path) = 0;

    // Clears persisted prior-failure state after a successful open.
    virtual void aggregate_opened() = 0;

    // Best-effort removes an aggregate after a destructive validation error.
    virtual void remove_aggregate(std::string_view path) = 0;

    // Publishes volatile update phase 3.
    virtual void publish_error() = 0;

    // Requests the shared rate-limited damaged-package broadcast.
    virtual void broadcast_validation_error(
        std::uint64_t now_milliseconds) = 0;

    // Sends one exact packet toward the controller.
    virtual void send_controller_packet(std::uint8_t type,
                                        core::BytesView payload) = 0;

    // Delegates structural bootable-image validation to the ESP32-S3 adapter.
    virtual bool valid_mainboard_image(core::BytesView image) = 0;
};

// Orchestrates one aggregate cleanup, load, and validation operation.
class UpdateValidationService {
public:
    // Binds validation ordering to replaceable storage and state adapters.
    explicit UpdateValidationService(UpdateValidationPort& port);

    // Returns one validated package or applies the exact selected failure path.
    std::optional<ValidatedUpdatePackage> validate(
        std::uint64_t now_milliseconds);

private:
    // Applies broadcast, deletion, and phase publication for format errors.
    void reject_format(std::uint64_t now_milliseconds);

    UpdateValidationPort& port_;
};

}  // namespace firmware::application
