// Implements bounded POSIX file reads and serialized controller UART replies.
#include "controller_transfer_adapter.hpp"

#include "controller_uart_adapter.hpp"

#include "firmware/core/frame.hpp"

#include <cstdio>
#include <climits>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace firmware::target {

ControllerTransferAdapter::ControllerTransferAdapter(ControllerUartAdapter& uart)
    : uart_(uart) {}

bool ControllerTransferAdapter::file_exists(std::string_view path) {
    struct stat information{};
    return stat(std::string(path).c_str(), &information) == 0 &&
           S_ISREG(information.st_mode);
}

std::optional<std::uint64_t> ControllerTransferAdapter::file_size(
    std::string_view path) {
    struct stat information{};
    if (stat(std::string(path).c_str(), &information) != 0 ||
        !S_ISREG(information.st_mode) || information.st_size < 0) {
        return std::nullopt;
    }
    return static_cast<std::uint64_t>(information.st_size);
}

std::optional<firmware::core::ByteVector> ControllerTransferAdapter::read_file(
    std::string_view path, std::uint64_t offset, std::size_t maximum_size) {
    if (maximum_size == 0U || offset > static_cast<std::uint64_t>(LONG_MAX)) {
        return std::nullopt;
    }
    std::FILE* file = std::fopen(std::string(path).c_str(), "rb");
    if (file == nullptr ||
        std::fseek(file, static_cast<long>(offset), SEEK_SET) != 0) {
        if (file != nullptr) std::fclose(file);
        return std::nullopt;
    }
    firmware::core::ByteVector data(maximum_size);
    const std::size_t count = std::fread(data.data(), 1U, maximum_size, file);
    const bool failed = std::ferror(file) != 0;
    std::fclose(file);
    if (failed) return std::nullopt;
    data.resize(count);
    return data;
}

std::optional<std::vector<firmware::core::ByteVector>>
ControllerTransferAdapter::read_chunks(std::string_view path,
                                        std::size_t chunk_size) {
    const auto size = file_size(path);
    if (!size.has_value() || chunk_size == 0U) return std::nullopt;
    std::vector<firmware::core::ByteVector> chunks;
    for (std::uint64_t offset = 0U; offset < *size;) {
        const auto chunk = read_file(path, offset, chunk_size);
        if (!chunk.has_value() || chunk->empty()) return std::nullopt;
        chunks.push_back(*chunk);
        offset += chunk->size();
    }
    return chunks;
}

bool ControllerTransferAdapter::remove_file(std::string_view path) {
    return unlink(std::string(path).c_str()) == 0;
}

bool ControllerTransferAdapter::send(firmware::core::Frame frame) {
    const auto encoded = firmware::core::encode_frame(frame);
    return !encoded.empty() && uart_.write(encoded);
}

void ControllerTransferAdapter::publish(
    firmware::application::FirmwareTransferEvent, std::uint32_t,
    std::uint32_t) {}

}  // namespace firmware::target
