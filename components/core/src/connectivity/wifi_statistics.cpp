// Implements stable JSON formatting for a Wi-Fi diagnostic snapshot.
#include "firmware/core/wifi_statistics.hpp"

#include <cstdio>
#include <string_view>

namespace firmware::core {
namespace {

void append_json_string(std::string& output, std::string_view value) {
    output.push_back('"');
    for (const unsigned char character : value) {
        switch (character) {
            case '"': output.append("\\\""); break;
            case '\\': output.append("\\\\"); break;
            case '\b': output.append("\\b"); break;
            case '\f': output.append("\\f"); break;
            case '\n': output.append("\\n"); break;
            case '\r': output.append("\\r"); break;
            case '\t': output.append("\\t"); break;
            default:
                if (character < 0x20U) {
                    char escaped[7]{};
                    std::snprintf(escaped, sizeof(escaped), "\\u%04x", character);
                    output.append(escaped);
                } else {
                    output.push_back(static_cast<char>(character));
                }
        }
    }
    output.push_back('"');
}

}  // namespace

std::string format_wifi_statistics_json(const WifiStatistics& statistics) {
    std::string output;
    output.reserve(320U + statistics.recent_events.size());
    output.append("{\"connected\":");
    output.append(statistics.connected ? "true" : "false");
    output.append(",\"rssi_dbm\":").append(std::to_string(statistics.rssi_dbm));
    output.append(",\"channel\":").append(std::to_string(statistics.channel));
    output.append(",\"authentication\":");
    append_json_string(output, statistics.authentication);
    output.append(",\"ipv4_address\":");
    append_json_string(output, statistics.ipv4_address);
    output.append(",\"station_starts\":").append(
        std::to_string(statistics.station_starts));
    output.append(",\"associations\":").append(
        std::to_string(statistics.associations));
    output.append(",\"disconnections\":").append(
        std::to_string(statistics.disconnections));
    output.append(",\"addresses_acquired\":").append(
        std::to_string(statistics.addresses_acquired));
    output.append(",\"addresses_lost\":").append(
        std::to_string(statistics.addresses_lost));
    output.append(",\"last_disconnect_reason\":").append(
        std::to_string(statistics.last_disconnect_reason));
    output.append(",\"reset_reason\":").append(
        std::to_string(statistics.reset_reason));
    output.append(",\"recent_events\":");
    append_json_string(output, statistics.recent_events);
    output.push_back('}');
    return output;
}

}  // namespace firmware::core
