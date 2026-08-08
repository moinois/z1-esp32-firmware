/** @file @brief Declares the nonlogging target file port for optional serial.log mirroring. */
#pragma once

#include "firmware/application/serial_diagnostic_log.hpp"

#include <cstdio>

namespace firmware::target {

/** Mirrors target serial diagnostics into the shared persistent log service. */
class SerialLogMirrorAdapter final
    : public firmware::application::SerialDiagnosticLogPort {
public:
    /// Offers one captured record to the opt-in bounded mirror.
    void write_record(firmware::core::BytesView record);

    /// Closes the stream before its mounted volume disappears.
    void storage_unmounted();

    /// Opens only an already-created regular sentinel file for append.
    std::optional<std::uint64_t> open_existing_append(
        std::string_view path) override;

    /// Writes bytes without producing diagnostics that could recurse.
    std::size_t write(firmware::core::BytesView record) override;

    /// Flushes the stream without logging failures.
    void flush() override;

    /// Closes the stream without logging failures.
    void close() override;

private:
    std::FILE* file_ = nullptr;
    firmware::application::SerialDiagnosticLogWriter writer_;
};

/// Returns the single mirror shared by the selected live or mock SD adapter.
SerialLogMirrorAdapter& serial_log_mirror();

}  // namespace firmware::target
