/** @file @brief Implements the logical SD root and canonical G-code directory mapping. */
#include "core/filesystem/sd_user_path.hpp"

#include "core/protocol/text.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace firmware::core {
namespace {

constexpr std::string_view gcodes_component = "gcodes";

// Splits one already normalized absolute path into nonempty components.
std::vector<std::string_view> components(std::string_view path) {
    std::vector<std::string_view> result;
    for (std::size_t start = 1U; start < path.size();) {
        const std::size_t end = path.find('/', start);
        result.push_back(path.substr(
            start, (end == std::string_view::npos ? path.size() : end) - start));
        if (end == std::string_view::npos) break;
        start = end + 1U;
    }
    return result;
}

// Joins a component suffix beneath one fixed physical root.
std::string join_under(std::string_view root,
                       const std::vector<std::string_view>& parts,
                       std::size_t first) {
    std::string result(root);
    for (std::size_t index = first; index < parts.size(); ++index) {
        result.push_back('/');
        result.append(parts[index]);
    }
    return result;
}

}  // namespace

std::string physical_sd_path(std::string_view logical_path) {
    if (logical_path.empty() || logical_path == "/") {
        return std::string(sd_mount_path);
    }
    std::string result(sd_mount_path);
    if (logical_path.front() != '/') result.push_back('/');
    result.append(logical_path);
    return result;
}

std::string resolve_sd_user_path(std::string_view path) {
    const std::string normalized = normalize_path(std::string(path));
    const std::vector<std::string_view> parts = components(normalized);

    for (std::size_t index = 0U; index < parts.size(); ++index) {
        if (parts[index] == gcodes_component) {
            return join_under(physical_sd_path("/gcodes"), parts, index + 1U);
        }
    }

    if (!parts.empty() && parts.front() == "sd") {
        return join_under(sd_mount_path, parts, 1U);
    }
    return join_under(sd_mount_path, parts, 0U);
}

std::string logical_sd_path(std::string_view physical_path) {
    if (physical_path == sd_mount_path) return "/";
    if (physical_path.size() >= sd_mount_prefix.size() &&
        physical_path.substr(0U, sd_mount_prefix.size()) == sd_mount_prefix) {
        return std::string(physical_path.substr(sd_mount_path.size()));
    }
    return std::string(physical_path);
}

}  // namespace firmware::core
