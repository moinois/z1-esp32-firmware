// Implements exact startup and sampled CAN digital-output diagnostics.
#include "firmware/application/can_output_monitor.hpp"

#include <cinttypes>
#include <cstdio>
#include <cstdint>

namespace firmware::application {
namespace {

constexpr std::size_t formatted_message_capacity = 48U;
constexpr std::size_t expected_output_size = 4U;
constexpr std::uint8_t bits_per_byte = 8U;
constexpr std::uint8_t observed_output_bit = 1U;

}  // namespace

CanOutputMonitor::CanOutputMonitor(
    const core::CanopenObjectDictionary& dictionary,
    CanOutputMonitorPort& port)
    : dictionary_(dictionary), port_(port) {}

void CanOutputMonitor::start() {
    previous_value_.reset();
    port_.log_info(can_output_monitor::diagnostic_tag,
                   can_output_monitor::startup_message);
    port_.log_info(can_output_monitor::diagnostic_tag,
                   can_output_monitor::gpio_message);
}

void CanOutputMonitor::sample() {
    const std::optional<std::uint32_t> value = output_value();
    if (!value.has_value() || value == previous_value_) {
        return;
    }

    char message[formatted_message_capacity];
    const std::uint32_t bit = (*value >> observed_output_bit) & 1U;
    const int length = std::snprintf(
        message,
        sizeof(message),
        "0x6001:1 DO=0x%08" PRIx32 " (DO2=%" PRIu32 ")",
        *value,
        bit);
    if (length > 0 && static_cast<std::size_t>(length) < sizeof(message)) {
        port_.log_info(can_output_monitor::diagnostic_tag,
                       std::string_view(message,
                                        static_cast<std::size_t>(length)));
        previous_value_ = value;
    }
}

std::optional<std::uint32_t> CanOutputMonitor::output_value() const {
    const core::DictionaryReadResult result = dictionary_.read(
        core::canopen_object::digital_output, 1U);
    if (result.abort != core::SdoAbort::none ||
        result.data.size() != expected_output_size) {
        return std::nullopt;
    }

    std::uint32_t value = 0U;
    for (std::size_t offset = 0U; offset < result.data.size(); ++offset) {
        value |= static_cast<std::uint32_t>(result.data[offset]) <<
                 (bits_per_byte * offset);
    }
    return value;
}

}  // namespace firmware::application
