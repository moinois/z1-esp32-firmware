/** @file @brief Implements bounded configuration reads, rewrites, and TCP response delivery. */
#include "tcp_configuration_adapter.hpp"

#include "configuration_file_store.hpp"

#include "application/transport/tcp_client_session.hpp"

#include <algorithm>
#include <string>

namespace firmware::target {

TcpConfigurationAdapter::TcpConfigurationAdapter(
    firmware::application::TcpClientSession& session) : session_(session) {}

TcpConfigurationAdapter::~TcpConfigurationAdapter() = default;

std::optional<std::vector<firmware::core::ByteVector>>
TcpConfigurationAdapter::read_configuration_chunks(
    std::size_t maximum_chunk_size) {
    if (maximum_chunk_size == 0U) return std::nullopt;
    const auto lines = ConfigurationFileStore{}.read_lines();
    std::string content;
    for (const auto& line : lines) {
        content += line;
        content.push_back('\n');
    }
    std::vector<firmware::core::ByteVector> chunks;
    for (std::size_t offset = 0U; offset < content.size();
         offset += maximum_chunk_size) {
        const std::size_t count =
            std::min(maximum_chunk_size, content.size() - offset);
        chunks.emplace_back(content.begin() + offset,
                            content.begin() + offset + count);
    }
    return chunks;
}

std::optional<std::string> TcpConfigurationAdapter::read_value(
    std::string_view tag, std::string_view key) {
    return ConfigurationFileStore{}.get_hashed(tag, key);
}

bool TcpConfigurationAdapter::set_value(std::string_view tag,
                                        std::string_view key,
                                        std::string_view value) {
    return ConfigurationFileStore{}.set(tag, key, value);
}

void TcpConfigurationAdapter::send(firmware::core::Frame frame) {
    static_cast<void>(session_.queue_frame(frame));
}

}  // namespace firmware::target
