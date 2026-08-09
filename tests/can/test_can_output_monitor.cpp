// Tests sampled CAN digital-output diagnostics and change collapsing.
#include "test.hpp"

#include "application/can/can_output_monitor.hpp"

#include <string>
#include <utility>
#include <vector>

using firmware::application::CanOutputMonitor;
using firmware::application::CanOutputMonitorPort;
using firmware::core::ByteVector;
using firmware::core::CanopenObjectDictionary;

namespace {

class FakeCanOutputMonitorPort final : public CanOutputMonitorPort {
public:
    // Captures one informational diagnostic record.
    void log_info(std::string_view tag, std::string_view message) override {
        records.emplace_back(std::string(tag), std::string(message));
    }

    std::vector<std::pair<std::string, std::string>> records;
};

// Encodes a 32-bit output value in dictionary byte order.
ByteVector le32(std::uint32_t value) {
    return {static_cast<std::uint8_t>(value),
            static_cast<std::uint8_t>(value >> 8U),
            static_cast<std::uint8_t>(value >> 16U),
            static_cast<std::uint8_t>(value >> 24U)};
}

}  // namespace

TEST_CASE(diag_033_monitor_start_emits_both_exact_information_records) {
    CanopenObjectDictionary dictionary;
    FakeCanOutputMonitorPort port;
    CanOutputMonitor monitor(dictionary, port);

    monitor.start();

    REQUIRE_EQ(port.records.size(), 2U);
    REQUIRE_EQ(port.records[0].first, std::string("APP_CO_DIO"));
    REQUIRE_EQ(port.records[0].second,
               std::string("0x6001:1 监视任务启动: DO2=bit1；本机 NodeID=17 → "
                           "仅当 SDO 目标为 0x611 或已配置 RPDO 时 OD 才会变，"
                           "继电器才动作"));
    REQUIRE_EQ(port.records[1].second,
               std::string("DO2 GPIO=-1（<0 则只打印 OD 不驱动引脚）"));
}

TEST_CASE(diag_033_first_sample_logs_zero_then_unchanged_samples_are_silent) {
    CanopenObjectDictionary dictionary;
    FakeCanOutputMonitorPort port;
    CanOutputMonitor monitor(dictionary, port);
    monitor.start();

    monitor.sample();
    monitor.sample();

    REQUIRE_EQ(port.records.size(), 3U);
    REQUIRE_EQ(port.records.back().second,
               std::string("0x6001:1 DO=0x00000000 (DO2=0)"));
}

TEST_CASE(diag_033_sampling_collapses_changes_and_formats_bit_one) {
    CanopenObjectDictionary dictionary;
    FakeCanOutputMonitorPort port;
    CanOutputMonitor monitor(dictionary, port);
    monitor.start();
    monitor.sample();
    dictionary.write(0x6001U, 1U, le32(1U));
    dictionary.write(0x6001U, 1U, le32(0xabcdef02U));

    monitor.sample();

    REQUIRE_EQ(port.records.size(), 4U);
    REQUIRE_EQ(port.records.back().second,
               std::string("0x6001:1 DO=0xabcdef02 (DO2=1)"));
    REQUIRE_EQ(firmware::application::can_output_monitor::sample_period_milliseconds,
               50U);
}
