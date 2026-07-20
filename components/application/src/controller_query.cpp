// Implements drift-free, non-deferred controller query scheduling.
#include "firmware/application/controller_query.hpp"

#include "firmware/core/protocol_constants.hpp"

namespace firmware::application {
namespace {

constexpr std::uint64_t status_period_milliseconds = 300U;
constexpr std::uint64_t diagnostic_period_milliseconds = 500U;

// Advances a periodic deadline past now and reports whether an opportunity elapsed.
bool consume_due_opportunity(std::uint64_t now, std::uint64_t period, std::uint64_t& next) {
    if (now < next) {
        return false;
    }
    const std::uint64_t elapsed_periods = ((now - next) / period) + 1U;
    next += elapsed_periods * period;
    return true;
}

}  // namespace

ControllerQueryScheduler::ControllerQueryScheduler(std::uint64_t start_milliseconds)
    : next_status_milliseconds_(start_milliseconds),
      next_diagnostic_milliseconds_(start_milliseconds) {}

std::vector<core::Frame> ControllerQueryScheduler::poll(std::uint64_t now_milliseconds,
                                                        bool controller_traffic_allowed) {
    const bool status_due =
        consume_due_opportunity(now_milliseconds, status_period_milliseconds, next_status_milliseconds_);
    const bool diagnostic_due = consume_due_opportunity(
        now_milliseconds, diagnostic_period_milliseconds, next_diagnostic_milliseconds_);

    std::vector<core::Frame> queries;
    if (!controller_traffic_allowed) {
        return queries;
    }
    if (status_due) {
        queries.push_back({core::protocol::single_command, {'?'}});
    }
    if (diagnostic_due) {
        queries.push_back(
            {core::protocol::general_command,
             {'d', 'i', 'a', 'g', 'n', 'o', 's', 'e', 0}});
    }
    return queries;
}

}  // namespace firmware::application
