// Implements bounded configuration reads, rewrites, and TCP response delivery.
#include "tcp_configuration_adapter.hpp"

#include "firmware/application/tcp_client_session.hpp"

#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace firmware::target {

TcpConfigurationAdapter::TcpConfigurationAdapter(
    firmware::application::TcpClientSession& session) : session_(session) {}

TcpConfigurationAdapter::~TcpConfigurationAdapter() = default;

std::optional<std::vector<firmware::core::ByteVector>>
TcpConfigurationAdapter::read_chunks(std::string_view path,
                                     std::size_t maximum_chunk_size) {
    if (maximum_chunk_size == 0U) return std::nullopt;
    std::FILE* file = std::fopen(std::string(path).c_str(), "rb");
    if (file == nullptr) return std::nullopt;
    std::vector<firmware::core::ByteVector> chunks;
    for (;;) {
        firmware::core::ByteVector chunk(maximum_chunk_size);
        const std::size_t count =
            std::fread(chunk.data(), 1U, chunk.size(), file);
        if (std::ferror(file) != 0) {
            std::fclose(file);
            return std::nullopt;
        }
        if (count == 0U) break;
        chunk.resize(count);
        chunks.push_back(std::move(chunk));
    }
    std::fclose(file);
    return chunks;
}

std::optional<std::string> TcpConfigurationAdapter::read_active_text(
    std::string_view path) {
    const auto chunks = read_chunks(path, 512U);
    if (!chunks.has_value()) return std::nullopt;
    std::string result;
    for (const auto& chunk : *chunks) {
        result.append(reinterpret_cast<const char*>(chunk.data()), chunk.size());
    }
    return result;
}

std::optional<std::vector<std::string>> TcpConfigurationAdapter::read_sd_lines(
    std::string_view path) {
    const auto content = read_active_text(path);
    if (!content.has_value()) return std::nullopt;
    std::vector<std::string> lines;
    std::size_t start = 0U;
    while (start <= content->size()) {
        const std::size_t end = content->find('\n', start);
        const std::size_t limit = end == std::string::npos ? content->size() : end;
        lines.push_back(content->substr(start, limit - start));
        if (end == std::string::npos) break;
        start = end + 1U;
    }
    return lines;
}

bool TcpConfigurationAdapter::write_temporary(std::string_view path,
                                              std::string_view content) {
    std::FILE* file = std::fopen(std::string(path).c_str(), "wb");
    if (file == nullptr) return false;
    const bool written = std::fwrite(content.data(), 1U, content.size(), file) ==
                         content.size();
    const bool flushed = std::fflush(file) == 0;
    const bool closed = std::fclose(file) == 0;
    return written && flushed && closed;
}

bool TcpConfigurationAdapter::unlink_active(std::string_view path) {
    return unlink(std::string(path).c_str()) == 0;
}

bool TcpConfigurationAdapter::rename_temporary(std::string_view source,
                                               std::string_view destination) {
    return rename(std::string(source).c_str(), std::string(destination).c_str()) == 0;
}

void TcpConfigurationAdapter::remove_temporary(std::string_view path) {
    static_cast<void>(unlink(std::string(path).c_str()));
}

void TcpConfigurationAdapter::send(firmware::core::Frame frame) {
    static_cast<void>(session_.queue_frame(frame));
}

}  // namespace firmware::target
