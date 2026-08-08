/** @file @brief Implements bounded serial.log writes without feeding errors back into ESP_LOG. */
#include "serial_log_mirror_adapter.hpp"

#include "sd_access_diagnostics.hpp"

#include <string>
#include <sys/stat.h>

namespace firmware::target {
namespace {

constexpr std::size_t stream_buffer_size = 4096U;

}  // namespace

void SerialLogMirrorAdapter::write_record(firmware::core::BytesView record) {
    if (sd_storage_mounted()) {
        writer_.write_record(record, *this);
    } else {
        storage_unmounted();
    }
}

void SerialLogMirrorAdapter::storage_unmounted() {
    writer_.disable(*this);
}

std::optional<std::uint64_t> SerialLogMirrorAdapter::open_existing_append(
    std::string_view path) {
    close();
    const std::string owned_path(path);
    struct stat status {};
    if (stat(owned_path.c_str(), &status) != 0 || !S_ISREG(status.st_mode)) {
        return std::nullopt;
    }
    file_ = std::fopen(owned_path.c_str(), "ab");
    if (file_ == nullptr) {
        return std::nullopt;
    }
    static_cast<void>(std::setvbuf(file_, nullptr, _IOFBF, stream_buffer_size));
    if (std::fseek(file_, 0L, SEEK_END) != 0) {
        close();
        return std::nullopt;
    }
    const long position = std::ftell(file_);
    if (position < 0L) {
        close();
        return std::nullopt;
    }
    return static_cast<std::uint64_t>(position);
}

std::size_t SerialLogMirrorAdapter::write(firmware::core::BytesView record) {
    return file_ == nullptr
               ? 0U
               : std::fwrite(record.data(), 1U, record.size(), file_);
}

void SerialLogMirrorAdapter::flush() {
    if (file_ != nullptr) {
        static_cast<void>(std::fflush(file_));
    }
}

void SerialLogMirrorAdapter::close() {
    if (file_ != nullptr) {
        static_cast<void>(std::fclose(file_));
        file_ = nullptr;
    }
}

SerialLogMirrorAdapter& serial_log_mirror() {
    static SerialLogMirrorAdapter adapter;
    return adapter;
}

}  // namespace firmware::target
