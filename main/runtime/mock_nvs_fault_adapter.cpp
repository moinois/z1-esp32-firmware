/** @file @brief Implements deterministic, nonpersistent NVS fault controls for mock builds. */
#include "mock_nvs_fault_adapter.hpp"

#include "hardware_adapter_factory.hpp"
#include "application/runtime/nvs_fault_injection.hpp"

namespace firmware::target {
namespace {

firmware::application::NvsFaultInjection faults;

}  // namespace

bool nvs_open_fault_active() {
    return HardwareAdapterFactory::nvs_faults_enabled() && faults.fail_open();
}

bool nvs_commit_fault_active() {
    return HardwareAdapterFactory::nvs_faults_enabled() && faults.fail_commit();
}

std::string handle_mock_nvs_control(std::string_view command) {
    if (!HardwareAdapterFactory::nvs_faults_enabled()) {
        return "mock-nvs unavailable in live build\n";
    }
    constexpr std::string_view prefix = "mock-nvs";
    std::string_view action = command.substr(
        command.size() < prefix.size() ? command.size() : prefix.size());
    while (!action.empty() && action.front() == ' ') {
        action.remove_prefix(1U);
    }
    if (action == "fail-open") {
        faults.select(firmware::application::NvsFault::open);
        return "mock-nvs fail-open selected\n";
    }
    if (action == "fail-commit") {
        faults.select(firmware::application::NvsFault::commit);
        return "mock-nvs fail-commit selected\n";
    }
    if (action == "clear") {
        faults.select(firmware::application::NvsFault::none);
        return "mock-nvs faults cleared\n";
    }
    return "mock-nvs expected fail-open, fail-commit, or clear\n";
}

}  // namespace firmware::target
