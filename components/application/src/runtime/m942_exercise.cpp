// Implements M942 routing independence and deadline-bounded SDO exercise cycles.
#include "firmware/application/m942_exercise.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace firmware::application {
namespace {

constexpr std::uint32_t low_nibble_mask = 0x0fU;
constexpr std::uint32_t high_nibble_bit = 0x08U;
constexpr std::uint8_t zero_reads_before_all_outputs = 10U;

// Creates the immutable local acknowledgement without retaining host state.
core::Frame acknowledgement() {
    constexpr std::string_view response = m942::response_text;
    return {m942::response_packet_type,
            {response.begin(), response.end()}};
}

}  // namespace

M942ExerciseService::M942ExerciseService(M942ExercisePort& port)
    : port_(port) {}

bool M942ExerciseService::submit(const HostIdentity& host,
                                 const core::Frame& frame,
                                 bool local_capacity_accepted) {
    port_.forward_to_controller(frame);
    if (!local_capacity_accepted ||
        frame.payload.size() > m942::maximum_command_payload_size) {
        return false;
    }

    port_.respond(host, acknowledgement());
    bool expected_inactive = false;
    if (!exercise_active_.compare_exchange_strong(expected_inactive, true)) {
        return false;
    }
    deadline_milliseconds_ =
        port_.monotonic_milliseconds() + m942::exercise_duration_milliseconds;
    consecutive_zero_reads_ = 0U;
    return true;
}

void M942ExerciseService::run() {
    if (!exercise_active_.load()) {
        return;
    }
    if (port_.monotonic_milliseconds() >= deadline_milliseconds_) {
        exercise_active_.store(false);
        return;
    }

    delay_bounded(m942::initial_delay_milliseconds);
    while (port_.monotonic_milliseconds() < deadline_milliseconds_) {
        port_.lock_sdo_client();
        if (port_.monotonic_milliseconds() >= deadline_milliseconds_) {
            port_.unlock_sdo_client();
            break;
        }

        const std::optional<std::uint32_t> input = read_with_retries();
        if (input.has_value() &&
            port_.monotonic_milliseconds() < deadline_milliseconds_) {
            write_with_retries(output_for_input(*input));
        }
        port_.unlock_sdo_client();

        if (port_.monotonic_milliseconds() < deadline_milliseconds_) {
            delay_bounded(m942::cycle_delay_milliseconds);
        }
    }
    exercise_active_.store(false);
}

bool M942ExerciseService::exercise_active() const {
    return exercise_active_.load();
}

std::optional<std::uint32_t> M942ExerciseService::read_with_retries() {
    for (std::size_t attempt = 0U;
         attempt < m942::maximum_sdo_attempts;
         ++attempt) {
        if (port_.monotonic_milliseconds() >= deadline_milliseconds_) {
            break;
        }
        const std::optional<std::uint32_t> value = port_.read_remote_u32(
            m942::remote_node_id,
            m942::remote_input_index,
            m942::remote_io_subindex,
            m942::sdo_protocol_timeout_milliseconds,
            deadline_milliseconds_);
        if (value.has_value()) {
            return value;
        }
        if (attempt + 1U < m942::maximum_sdo_attempts &&
            port_.monotonic_milliseconds() < deadline_milliseconds_) {
            delay_bounded(m942::sdo_retry_delay_milliseconds);
        }
    }
    return std::nullopt;
}

void M942ExerciseService::write_with_retries(std::uint32_t value) {
    for (std::size_t attempt = 0U;
         attempt < m942::maximum_sdo_attempts;
         ++attempt) {
        if (port_.monotonic_milliseconds() >= deadline_milliseconds_) {
            return;
        }
        if (port_.write_remote_u32(
                m942::remote_node_id,
                m942::remote_output_index,
                m942::remote_io_subindex,
                value,
                m942::sdo_protocol_timeout_milliseconds,
                deadline_milliseconds_)) {
            return;
        }
        if (attempt + 1U < m942::maximum_sdo_attempts &&
            port_.monotonic_milliseconds() < deadline_milliseconds_) {
            delay_bounded(m942::sdo_retry_delay_milliseconds);
        }
    }
}

std::uint32_t M942ExerciseService::output_for_input(std::uint32_t input) {
    const std::uint32_t nibble = input & low_nibble_mask;
    if (nibble == 0U) {
        consecutive_zero_reads_ = std::min<std::uint8_t>(
            static_cast<std::uint8_t>(consecutive_zero_reads_ + 1U),
            zero_reads_before_all_outputs);
        return consecutive_zero_reads_ >= zero_reads_before_all_outputs
                   ? low_nibble_mask
                   : 0U;
    }

    consecutive_zero_reads_ = 0U;
    const std::uint32_t rotated =
        ((nibble << 1U) | ((nibble & high_nibble_bit) >> 3U)) &
        low_nibble_mask;
    return nibble | rotated;
}

void M942ExerciseService::delay_bounded(
    std::uint32_t requested_milliseconds) {
    const std::uint64_t now = port_.monotonic_milliseconds();
    if (now >= deadline_milliseconds_) {
        return;
    }
    const std::uint64_t remaining = deadline_milliseconds_ - now;
    port_.delay_milliseconds(static_cast<std::uint32_t>(
        std::min<std::uint64_t>(requested_milliseconds, remaining)));
}

}  // namespace firmware::application
