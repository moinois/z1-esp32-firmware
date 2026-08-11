/** @file @brief Implements controller UART activity alarms and bounded output scheduling. */
#include "application/controller/controller_link.hpp"

#include "core/protocol/protocol_constants.hpp"

#include <utility>

namespace firmware::application {
namespace {

constexpr std::uint64_t inactivity_period_milliseconds = 10000U;
constexpr std::uint64_t write_interval_milliseconds = 10U;
constexpr std::size_t maximum_pending_items = 32U;
constexpr std::size_t maximum_item_size = core::protocol::controller_maximum_item_size;
constexpr char inactivity_alarm[] =
    "ALARM: Mainboard did not receive a status response from the CTRL (RX error)\n";

// Creates the exact controller-originated console frame required for inactivity.
core::Frame make_inactivity_alarm() {
    const auto* begin = reinterpret_cast<const std::uint8_t*>(inactivity_alarm);
    const auto* end = begin + sizeof(inactivity_alarm) - 1U;
    return {core::protocol::console_message, core::ByteVector(begin, end)};
}

}  // namespace

ControllerActivityMonitor::ControllerActivityMonitor(std::uint64_t start_milliseconds)
    : next_alarm_milliseconds_(start_milliseconds + inactivity_period_milliseconds) {}

void ControllerActivityMonitor::record_valid_frame(std::uint64_t now_milliseconds) {
    next_alarm_milliseconds_ = now_milliseconds + inactivity_period_milliseconds;
}

std::vector<core::Frame> ControllerActivityMonitor::poll(std::uint64_t now_milliseconds) {
    std::vector<core::Frame> alarms;
    if (now_milliseconds < next_alarm_milliseconds_) {
        return alarms;
    }

    const std::uint64_t elapsed_periods =
        ((now_milliseconds - next_alarm_milliseconds_) / inactivity_period_milliseconds) + 1U;
    alarms.reserve(static_cast<std::size_t>(elapsed_periods));
    for (std::uint64_t period = 0U; period < elapsed_periods; ++period) {
        alarms.push_back(make_inactivity_alarm());
    }
    next_alarm_milliseconds_ += elapsed_periods * inactivity_period_milliseconds;
    return alarms;
}

bool ControllerOutputQueue::enqueue(core::ByteVector item) {
    if (item.empty() || item.size() > maximum_item_size || items_.size() >= maximum_pending_items) {
        return false;
    }
    items_.push_back(std::move(item));
    return true;
}

std::optional<core::ByteVector> ControllerOutputQueue::take_ready(std::uint64_t now_milliseconds) {
    if (items_.empty() || now_milliseconds < next_write_milliseconds_) {
        return std::nullopt;
    }

    core::ByteVector item = std::move(items_.front());
    items_.pop_front();
    next_write_milliseconds_ = now_milliseconds + write_interval_milliseconds;
    return item;
}

std::optional<DiagnosticMessage> ControllerOutputQueue::record_write_result(int bytes_written) const {
    if (bytes_written < 0) {
        return DiagnosticMessage{"uart_task", "UART send failed"};
    }
    return std::nullopt;
}

std::size_t ControllerOutputQueue::pending() const {
    return items_.size();
}

bool ControllerOutputQueue::full() const {
    return items_.size() >= maximum_pending_items;
}

}  // namespace firmware::application
