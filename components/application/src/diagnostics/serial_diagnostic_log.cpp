// Implements opt-in serial.log mirroring without changing the UART destination.
#include "firmware/application/serial_diagnostic_log.hpp"

#include "firmware/core/sd_user_path.hpp"

#include <string>

namespace firmware::application {
namespace {

const std::string serial_log_path = core::physical_sd_path("/serial.log");

}  // namespace

void SerialDiagnosticLogWriter::write_record(core::BytesView record,
                                             SerialDiagnosticLogPort& port) {
    if (record.size() == 0U) {
        return;
    }
    if (!active_) {
        const auto size = port.open_existing_append(serial_log_path);
        if (!size.has_value()) {
            return;
        }
        tracked_size_ = *size;
        active_ = true;
    }
    if (tracked_size_ > serial_diagnostic_log_maximum_size ||
        record.size() > serial_diagnostic_log_maximum_size - tracked_size_) {
        ++dropped_record_count_;
        disable(port);
        return;
    }

    const std::size_t written = port.write(record);
    tracked_size_ += written;
    if (written != record.size()) {
        ++dropped_record_count_;
        disable(port);
        return;
    }
    port.flush();
    // FatFS can reject a second reader while the append stream remains open.
    // Close after every durable record so file-transfer downloads and cleanup
    // can access the opt-in sentinel between diagnostic writes.
    disable(port);
}

void SerialDiagnosticLogWriter::disable(SerialDiagnosticLogPort& port) {
    if (active_) {
        port.close();
    }
    active_ = false;
}

bool SerialDiagnosticLogWriter::active() const {
    return active_;
}

std::uint64_t SerialDiagnosticLogWriter::tracked_size() const {
    return tracked_size_;
}

std::uint64_t SerialDiagnosticLogWriter::dropped_record_count() const {
    return dropped_record_count_;
}

}  // namespace firmware::application
