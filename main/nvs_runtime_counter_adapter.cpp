// Implements runtime-counter persistence over the shared NVS adapter.
#include "nvs_runtime_counter_adapter.hpp"

namespace firmware::target {

firmware::application::FirstBootRead NvsRuntimeCounterAdapter::read_first_boot(
    std::string_view name_space, std::string_view key) {
    const auto value = nvs_.read_u64_state(name_space, key);
    if (value.state == NvsReadState::found) {
        return {firmware::application::FirstBootReadResult::present,
                static_cast<std::int64_t>(value.value)};
    }
    if (value.state == NvsReadState::missing) {
        return {firmware::application::FirstBootReadResult::missing, 0};
    }
    return {firmware::application::FirstBootReadResult::failure, 0};
}

std::optional<std::uint64_t> NvsRuntimeCounterAdapter::read_counter(
    std::string_view name_space, std::string_view key) {
    return nvs_.read_u64(name_space, key);
}

bool NvsRuntimeCounterAdapter::write_first_boot(std::string_view name_space,
                                                std::string_view key,
                                                std::int64_t seconds) {
    return nvs_.write_i64(name_space, key, seconds);
}

bool NvsRuntimeCounterAdapter::write_counter(std::string_view name_space,
                                             std::string_view key,
                                             std::uint64_t value) {
    return nvs_.write_u64(name_space, key, value);
}

}  // namespace firmware::target
