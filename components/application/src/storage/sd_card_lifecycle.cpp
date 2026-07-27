// Implements nonfatal mount lifecycle, three-sample debounce, and capacity policy.
#include "firmware/application/sd_card_lifecycle.hpp"
#include "firmware/core/sd_user_path.hpp"

namespace firmware::application {
namespace {

const SdMountConfig mount_config{
    core::sd_mount_path,
    false,
    16U,
    16U * 1024U,
};
constexpr std::uint64_t sample_period_milliseconds = 200U;
constexpr std::uint64_t insertion_delay_milliseconds = 100U;
constexpr std::uint8_t required_different_samples = 3U;
constexpr std::uint64_t bytes_per_mebibyte = 1024U * 1024U;

}  // namespace

void SdCardLifecycle::start(std::uint64_t now_milliseconds, SdCardPort& port) {
    mounted_ = false;
    settled_inserted_ = false;
    different_sample_count_ = 0U;
    mount_pending_ = false;
    next_sample_milliseconds_ = now_milliseconds + sample_period_milliseconds;
    started_ = true;

    if (port.card_inserted()) {
        attempt_mount(port);
    }
}

void SdCardLifecycle::poll(std::uint64_t now_milliseconds, SdCardPort& port) {
    if (!started_) {
        return;
    }
    if (mount_pending_ && now_milliseconds >= pending_mount_milliseconds_) {
        mount_pending_ = false;
        if (!mounted_) {
            attempt_mount(port);
        }
    }
    if (now_milliseconds < next_sample_milliseconds_) {
        return;
    }

    const std::uint64_t elapsed_periods =
        ((now_milliseconds - next_sample_milliseconds_) /
         sample_period_milliseconds) + 1U;
    next_sample_milliseconds_ += elapsed_periods * sample_period_milliseconds;

    const bool inserted = port.card_inserted();
    if (inserted == settled_inserted_) {
        different_sample_count_ = 0U;
        return;
    }
    ++different_sample_count_;
    if (different_sample_count_ < required_different_samples) {
        return;
    }

    settled_inserted_ = inserted;
    different_sample_count_ = 0U;
    if (inserted) {
        mount_pending_ = true;
        pending_mount_milliseconds_ =
            now_milliseconds + insertion_delay_milliseconds;
    } else {
        mount_pending_ = false;
        accept_removal(port);
    }
}

bool SdCardLifecycle::mounted() const {
    return mounted_;
}

std::optional<SdCapacity> SdCardLifecycle::read_capacity(SdCardPort& port) {
    const auto total = port.total_bytes();
    const auto free = port.free_bytes();
    if (!total.has_value() || !free.has_value()) {
        return std::nullopt;
    }
    return SdCapacity{
        *total / bytes_per_mebibyte,
        *free / bytes_per_mebibyte,
    };
}

void SdCardLifecycle::attempt_mount(SdCardPort& port) {
    if (!port.mount(mount_config)) {
        return;
    }
    mounted_ = true;
    port.start_logging();
}

void SdCardLifecycle::accept_removal(SdCardPort& port) {
    if (!mounted_) {
        return;
    }
    port.stop_and_drain_logging();
    if (port.unmount()) {
        mounted_ = false;
    }
}

}  // namespace firmware::application
