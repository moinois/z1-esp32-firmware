/** @file @brief Implements runtime-counter persistence over the shared NVS adapter. */
#include "nvs_runtime_counter_adapter.hpp"

#include "esp_err.h"
#include "esp_log.h"

namespace firmware::target {
namespace {

firmware::application::RuntimeMutationStage mutation_stage(
    NvsMutationStage stage) {
    switch (stage) {
        case NvsMutationStage::none:
            return firmware::application::RuntimeMutationStage::none;
        case NvsMutationStage::open:
            return firmware::application::RuntimeMutationStage::open;
        case NvsMutationStage::mutation:
            return firmware::application::RuntimeMutationStage::mutation;
        case NvsMutationStage::commit:
            return firmware::application::RuntimeMutationStage::commit;
    }
    return firmware::application::RuntimeMutationStage::commit;
}

}  // namespace

firmware::application::FirstBootRead NvsRuntimeCounterAdapter::read_first_boot(
    std::string_view name_space, std::string_view key) {
    const auto value = nvs_.read_u64_state(name_space, key);
    if (value.state == NvsReadState::found) {
        return {firmware::application::FirstBootReadResult::present,
                static_cast<std::int64_t>(value.value), false, {}};
    }
    if (value.state == NvsReadState::missing) {
        return {firmware::application::FirstBootReadResult::missing, 0,
                false, {}};
    }
    return {firmware::application::FirstBootReadResult::failure, 0,
            value.open_failed, esp_err_to_name(value.error)};
}

std::optional<std::uint64_t> NvsRuntimeCounterAdapter::read_counter(
    std::string_view name_space, std::string_view key) {
    return nvs_.read_u64(name_space, key);
}

firmware::application::RuntimeMutationResult
NvsRuntimeCounterAdapter::write_first_boot(std::string_view name_space,
                                           std::string_view key,
                                           std::int64_t seconds) {
    const auto result = nvs_.write_i64_detailed(name_space, key, seconds);
    return {mutation_stage(result.stage),
            esp_err_to_name(result.error)};
}

firmware::application::RuntimeMutationResult
NvsRuntimeCounterAdapter::write_counter(std::string_view name_space,
                                        std::string_view key,
                                        std::uint64_t value) {
    const auto result = nvs_.write_u64_detailed(name_space, key, value);
    return {mutation_stage(result.stage),
            esp_err_to_name(result.error)};
}

void NvsRuntimeCounterAdapter::diagnose(std::string_view message) {
    ESP_LOGW("APP_NVS", "%.*s", static_cast<int>(message.size()), message.data());
}

}  // namespace firmware::target
