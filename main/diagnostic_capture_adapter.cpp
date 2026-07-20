// Implements a nonblocking ESP-IDF vprintf hook over DiagnosticCapture.
#include "diagnostic_capture_adapter.hpp"

#include "esp_log.h"
#include "esp_timer.h"

#include "firmware/application/diagnostic_capture.hpp"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string_view>

namespace firmware::target {
namespace {

constexpr std::size_t capture_buffer_size = 512U;
vprintf_like_t original_vprintf = nullptr;
firmware::application::DiagnosticCapture capture;

class HookPort final : public firmware::application::DiagnosticCapturePort {
public:
    void print(firmware::core::BytesView) override {}
    bool record_buffer_available() override { return true; }
    firmware::application::DiagnosticTime current_time() override {
        return {0U, 0U, 0U, 0U, 0U, 0U, 0U,
                static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL)};
    }
};

int capture_vprintf(const char* format, va_list arguments) {
    va_list preserved;
    va_copy(preserved, arguments);
    const int printed = original_vprintf == nullptr ? 0 : original_vprintf(format, preserved);
    va_end(preserved);

    char buffer[capture_buffer_size];
    va_list copy;
    va_copy(copy, arguments);
    const int length = std::vsnprintf(buffer, sizeof(buffer), format, copy);
    va_end(copy);
    if (length > 0) {
        HookPort port;
        capture.capture(std::string_view(buffer), {}, port);
    }
    return printed;
}

}  // namespace

void DiagnosticCaptureAdapter::start() {
    capture.set_logging_active(true);
    original_vprintf = esp_log_set_vprintf(capture_vprintf);
}

std::optional<firmware::core::ByteVector> take_captured_diagnostic() {
    return capture.take_pending();
}

firmware::application::DiagnosticCapture& diagnostic_capture_state() {
    return capture;
}

}  // namespace firmware::target
