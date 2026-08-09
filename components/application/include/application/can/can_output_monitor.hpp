/** @file @brief Declares sampled diagnostics for the local CANopen digital-output object. */
#pragma once

#include "core/can/canopen_dictionary.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace firmware::application {

namespace can_output_monitor {

inline constexpr std::uint32_t sample_period_milliseconds = 50U;
inline constexpr std::string_view diagnostic_tag = "APP_CO_DIO";
inline constexpr std::string_view startup_message =
    "0x6001:1 监视任务启动: DO2=bit1；本机 NodeID=17 → "
    "仅当 SDO 目标为 0x611 或已配置 RPDO 时 OD 才会变，继电器才动作";
inline constexpr std::string_view gpio_message =
    "DO2 GPIO=-1（<0 则只打印 OD 不驱动引脚）";

}  // namespace can_output_monitor

/// Isolates sampled output diagnostics from the target logging implementation.
class CanOutputMonitorPort {
public:
    /// Enables safe destruction through a substituted diagnostic adapter.
    virtual ~CanOutputMonitorPort() = default;

    /// Emits one informational record under the supplied tag.
    virtual void log_info(std::string_view tag,
                          std::string_view message) = 0;
};

/// Collapses dictionary changes between nominal 50 ms observations.
class CanOutputMonitor {
public:
    /// Binds the monitor to read-only dictionary state and a log destination.
    CanOutputMonitor(const core::CanopenObjectDictionary& dictionary,
                     CanOutputMonitorPort& port);

    /// Emits the two exact startup records and resets first-sample state.
    void start();

    /// Logs the first or changed output value using exact lowercase formatting.
    void sample();

private:
    /// Reads the fixed 32-bit output object from its little-endian bytes.
    std::optional<std::uint32_t> output_value() const;

    const core::CanopenObjectDictionary& dictionary_;
    CanOutputMonitorPort& port_;
    std::optional<std::uint32_t> previous_value_;
};

}  // namespace firmware::application
