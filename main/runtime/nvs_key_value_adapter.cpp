/** @file @brief Implements typed NVS access with explicit missing-key and commit handling. */
#include "nvs_key_value_adapter.hpp"
#include "mock_nvs_fault_adapter.hpp"

#include "nvs.h"

#include <vector>

namespace firmware::target {
namespace {

constexpr nvs_open_mode_t read_only = NVS_READONLY;
constexpr nvs_open_mode_t read_write = NVS_READWRITE;

bool open_namespace(std::string_view name_space, nvs_open_mode_t mode,
                    nvs_handle_t& handle) {
    if (nvs_open_fault_active()) return false;
    return nvs_open(std::string(name_space).c_str(), mode, &handle) == ESP_OK;
}

// Commits one successful mutation unless the test boundary rejects it.
esp_err_t commit_mutation(nvs_handle_t handle, esp_err_t mutation_result) {
    if (mutation_result != ESP_OK) return mutation_result;
    return nvs_commit_fault_active() ? ESP_FAIL : nvs_commit(handle);
}

}  // namespace

NvsStringRead NvsKeyValueAdapter::read_string(std::string_view name_space,
                                               std::string_view key) const {
    if (nvs_open_fault_active()) return {};
    nvs_handle_t handle = 0;
    const esp_err_t open_result = nvs_open(
        std::string(name_space).c_str(), read_only, &handle);
    if (open_result == ESP_ERR_NVS_NOT_FOUND) {
        return {NvsReadState::missing, {}};
    }
    if (open_result != ESP_OK) return {};
    std::size_t size = 0U;
    const esp_err_t size_result = nvs_get_str(handle, std::string(key).c_str(), nullptr, &size);
    if (size_result == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return {NvsReadState::missing, {}};
    }
    if (size_result != ESP_OK || size == 0U) {
        nvs_close(handle);
        return {};
    }
    std::vector<char> buffer(size);
    const esp_err_t read_result = nvs_get_str(handle, std::string(key).c_str(), buffer.data(), &size);
    nvs_close(handle);
    if (read_result != ESP_OK) return {};
    return {NvsReadState::found, std::string(buffer.data())};
}

std::optional<std::uint64_t> NvsKeyValueAdapter::read_u64(
    std::string_view name_space, std::string_view key) const {
    const auto result = read_u64_state(name_space, key);
    return result.state == NvsReadState::found
               ? std::optional<std::uint64_t>(result.value)
               : std::nullopt;
}

NvsU64Read NvsKeyValueAdapter::read_u64_state(
    std::string_view name_space, std::string_view key) const {
    nvs_handle_t handle = 0;
    std::uint64_t value = 0U;
    if (!open_namespace(name_space, read_only, handle)) return {};
    const esp_err_t result = nvs_get_u64(handle, std::string(key).c_str(), &value);
    nvs_close(handle);
    if (result == ESP_OK) return {NvsReadState::found, value};
    if (result == ESP_ERR_NVS_NOT_FOUND) return {NvsReadState::missing, 0U};
    return {};
}

std::optional<std::uint8_t> NvsKeyValueAdapter::read_u8(
    std::string_view name_space, std::string_view key) const {
    nvs_handle_t handle = 0;
    if (!open_namespace(name_space, read_only, handle)) {
        return std::nullopt;
    }
    std::uint8_t value = 0U;
    const esp_err_t result = nvs_get_u8(handle, std::string(key).c_str(), &value);
    nvs_close(handle);
    return result == ESP_OK ? std::optional<std::uint8_t>(value) : std::nullopt;
}

std::optional<std::int64_t> NvsKeyValueAdapter::read_i64(
    std::string_view name_space, std::string_view key) const {
    const auto value = read_u64(name_space, key);
    return value.has_value() ? std::optional<std::int64_t>(static_cast<std::int64_t>(*value))
                             : std::nullopt;
}

bool NvsKeyValueAdapter::write_string(std::string_view name_space,
                                      std::string_view key,
                                      std::string_view value) const {
    nvs_handle_t handle = 0;
    if (!open_namespace(name_space, read_write, handle)) return false;
    // ESP-IDF keeps uncommitted mutations in a shared in-memory NVS cache;
    // closing the handle does not roll them back. Inject before nvs_set_* so a
    // simulated commit failure cannot become visible to subsequent reads.
    if (nvs_commit_fault_active()) {
        nvs_close(handle);
        return false;
    }
    const esp_err_t result = nvs_set_str(handle, std::string(key).c_str(), std::string(value).c_str());
    const esp_err_t commit = commit_mutation(handle, result);
    nvs_close(handle);
    return commit == ESP_OK;
}

bool NvsKeyValueAdapter::write_u64(std::string_view name_space,
                                   std::string_view key,
                                   std::uint64_t value) const {
    nvs_handle_t handle = 0;
    if (!open_namespace(name_space, read_write, handle)) return false;
    if (nvs_commit_fault_active()) {
        nvs_close(handle);
        return false;
    }
    const esp_err_t result = nvs_set_u64(handle, std::string(key).c_str(), value);
    const esp_err_t commit = commit_mutation(handle, result);
    nvs_close(handle);
    return commit == ESP_OK;
}

bool NvsKeyValueAdapter::write_u8(std::string_view name_space,
                                  std::string_view key, std::uint8_t value) const {
    nvs_handle_t handle = 0;
    if (!open_namespace(name_space, read_write, handle)) {
        return false;
    }
    if (nvs_commit_fault_active()) {
        nvs_close(handle);
        return false;
    }
    const esp_err_t result = nvs_set_u8(handle, std::string(key).c_str(), value);
    const esp_err_t commit = commit_mutation(handle, result);
    nvs_close(handle);
    return commit == ESP_OK;
}

bool NvsKeyValueAdapter::write_i64(std::string_view name_space,
                                   std::string_view key,
                                   std::int64_t value) const {
    return write_u64(name_space, key, static_cast<std::uint64_t>(value));
}

NvsReadState NvsKeyValueAdapter::erase_key(std::string_view name_space,
                                           std::string_view key) const {
    if (nvs_open_fault_active()) {
        return NvsReadState::failure;
    }
    nvs_handle_t handle = 0;
    const esp_err_t open_result = nvs_open(
        std::string(name_space).c_str(), NVS_READWRITE, &handle);
    if (open_result != ESP_OK) {
        return NvsReadState::failure;
    }
    if (nvs_commit_fault_active()) {
        nvs_close(handle);
        return NvsReadState::failure;
    }
    const esp_err_t erase_result = nvs_erase_key(handle,
                                                  std::string(key).c_str());
    if (erase_result == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return NvsReadState::missing;
    }
    if (erase_result != ESP_OK || nvs_commit_fault_active() ||
        nvs_commit(handle) != ESP_OK) {
        nvs_close(handle);
        return NvsReadState::failure;
    }
    nvs_close(handle);
    return NvsReadState::found;
}

}  // namespace firmware::target
