/** @file @brief Declares exact diagnostics for host file-transfer admission. */
#pragma once

#include "application/runtime/ownership.hpp"

#include <string>

namespace firmware::application {

/** Formats the DIAG-027 busy-reply record for one bounded host client ID.
 *
 * @param host Stable transport identity. TCP slots map to logical identifiers
 * `0x10` through `0x13`; USB maps to `0x01` as required by TRN-001.
 */
std::string file_transfer_busy_message(const HostIdentity& host);

}  // namespace firmware::application
