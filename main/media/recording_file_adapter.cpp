// Implements complete AVI segment writes through the ESP-IDF POSIX VFS.
#include "recording_file_adapter.hpp"

#include <cerrno>
#include <cstdio>
#include <string>
#include <unistd.h>

namespace firmware::target {

bool RecordingFileAdapter::write_segment(
    std::string_view path, firmware::core::BytesView data) const {
    std::FILE* file = std::fopen(std::string(path).c_str(), "wb");
    if (file == nullptr) {
        return false;
    }
    std::size_t written = 0U;
    while (written < data.size()) {
        const std::size_t count =
            std::fwrite(data.data() + written, 1U, data.size() - written, file);
        if (count == 0U) {
            std::fclose(file);
            return false;
        }
        written += count;
    }
    if (std::fflush(file) != 0 || fsync(fileno(file)) != 0) {
        std::fclose(file);
        return false;
    }
    return std::fclose(file) == 0;
}

}  // namespace firmware::target
