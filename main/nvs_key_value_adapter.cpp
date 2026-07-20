// Implements typed NVS access with explicit missing-key and commit handling.
#include "nvs_key_value_adapter.hpp"

#include "nvs.h"

#include <vector>

namespace firmware::target {
namespace {

constexpr nvs_open_mode_t read_only = NVS_READONLY;
constexpr nvs_open_mode_t read_write = NVS_READWRITE;

bool open_namespace(std::string_view name_space, nvs_open_mode_t mode,
                    nvs_handle_t& handle) {
    return nvs_open(std::string(name_space).c_str(), mode, &handle) == ESP_OK;
}

}  // namespace

NvsStringRead NvsKeyValueAdapter::read_string(std::string_view name_space,
                                               std::string_view key) const {
    nvs_handle_t handle = 0;
    if (!open_namespace(name_space, read_only, handle)) return {};
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
    nvs_handle_t handle = 0;
    std::uint64_t value = 0U;
    if (!open_namespace(name_space, read_only, handle)) return std::nullopt;
    const esp_err_t result = nvs_get_u64(handle, std::string(key).c_str(), &value);
    nvs_close(handle);
    return result == ESP_OK ? std::optional<std::uint64_t>(value) : std::nullopt;
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
    const esp_err_t result = nvs_set_str(handle, std::string(key).c_str(), std::string(value).c_str());
    const esp_err_t commit = result == ESP_OK ? nvs_commit(handle) : result;
    nvs_close(handle);
    return commit == ESP_OK;
}

bool NvsKeyValueAdapter::write_u64(std::string_view name_space,
                                   std::string_view key,
                                   std::uint64_t value) const {
    nvs_handle_t handle = 0;
    if (!open_namespace(name_space, read_write, handle)) return false;
    const esp_err_t result = nvs_set_u64(handle, std::string(key).c_str(), value);
    const esp_err_t commit = result == ESP_OK ? nvs_commit(handle) : result;
    nvs_close(handle);
    return commit == ESP_OK;
}

bool NvsKeyValueAdapter::write_i64(std::string_view name_space,
                                   std::string_view key,
                                   std::int64_t value) const {
    return write_u64(name_space, key, static_cast<std::uint64_t>(value));
}

}  // namespace firmware::target
