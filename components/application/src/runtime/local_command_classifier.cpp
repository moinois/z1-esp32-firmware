/** @file @brief Implements the stable mapping from command recognition to service families. */
#include "firmware/application/local_command_classifier.hpp"

namespace firmware::application {

LocalCommandFamily classify_local_command(core::CommandKind command) {
    switch (command) {
        case core::CommandKind::system_time:
        case core::CommandKind::clear_first_time:
            return LocalCommandFamily::runtime;
        case core::CommandKind::serial_get:
        case core::CommandKind::serial_set:
            return LocalCommandFamily::serial_number;
        case core::CommandKind::record_start:
        case core::CommandKind::record_stop:
            return LocalCommandFamily::recording;
        case core::CommandKind::list:
        case core::CommandKind::file_type:
        case core::CommandKind::md5_sum:
        case core::CommandKind::remove:
        case core::CommandKind::move:
            return LocalCommandFamily::filesystem;
        case core::CommandKind::wlan:
            return LocalCommandFamily::wlan;
        default:
            return LocalCommandFamily::none;
    }
}

}  // namespace firmware::application
