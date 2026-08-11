/** @file @brief Declares exact diagnostics for host file-transfer admission. */
#pragma once

#include "application/runtime/ownership.hpp"

#include <string>
#include <string_view>

namespace firmware::application {

/** Formats the DIAG-027 busy-reply record for one bounded host client ID.
 *
 * @param host Stable transport identity. TCP slots map to logical identifiers
 * `0x10` through `0x13`; USB maps to `0x01` as required by TRN-001.
 */
std::string file_transfer_busy_message(const HostIdentity& host);

/// One exact tagged DIAG-038 non-owner warning.
struct FileTransferDiagnostic {
    std::string_view tag;
    std::string message;
};

/// Formats the transport-specific warning for ignored non-owner file data.
FileTransferDiagnostic non_owner_file_data_diagnostic(
    const HostIdentity& source, const HostIdentity& owner);

}  // namespace firmware::application
