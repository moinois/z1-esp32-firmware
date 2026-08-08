/** @file @brief Implements byte-exact expedited SDO request decoding and response encoding. */
#include "firmware/core/canopen_sdo.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace firmware::core {
namespace {

constexpr std::uint8_t complete_sdo_frame_size = 8U;
constexpr std::uint8_t initiate_upload_request = 0x40U;
constexpr std::uint8_t initiate_upload_response = 0x43U;
constexpr std::uint8_t expedited_download_pattern = 0x23U;
constexpr std::uint8_t expedited_download_mask = 0xf3U;
constexpr std::uint8_t initiate_download_response = 0x60U;
constexpr std::uint8_t abort_transfer_response = 0x80U;
constexpr std::uint8_t command_offset = 0U;
constexpr std::uint8_t index_low_offset = 1U;
constexpr std::uint8_t index_high_offset = 2U;
constexpr std::uint8_t subindex_offset = 3U;
constexpr std::uint8_t data_offset = 4U;
constexpr std::uint8_t scalar_capacity = 4U;
constexpr std::uint8_t unused_byte_shift = 2U;
constexpr std::uint8_t unused_byte_mask = 0x03U;
constexpr std::uint8_t bits_per_byte = 8U;

// Reads the little-endian object index carried by every SDO request.
std::uint16_t object_index(const CanFrame& request) {
    return static_cast<std::uint16_t>(request.data[index_low_offset]) |
           (static_cast<std::uint16_t>(request.data[index_high_offset]) <<
            bits_per_byte);
}

}  // namespace

CanopenSdoServer::CanopenSdoServer(CanopenObjectDictionary& dictionary)
    : dictionary_(dictionary) {}

std::optional<SdoServerResult> CanopenSdoServer::handle(
    const CanFrame& request) {
    if (request.identifier != canopen::sdo_request_identifier ||
        request.size != complete_sdo_frame_size) {
        return std::nullopt;
    }

    const std::uint16_t index = object_index(request);
    const std::uint8_t subindex = request.data[subindex_offset];
    const std::uint8_t command = request.data[command_offset];
    if (command == initiate_upload_request) {
        const DictionaryReadResult read = dictionary_.read(index, subindex);
        if (read.abort != SdoAbort::none) {
            return abort_response(request, read.abort);
        }
        const std::uint8_t unused =
            static_cast<std::uint8_t>(scalar_capacity - read.data.size());
        CanFrame response = response_header(
            request, static_cast<std::uint8_t>(
                         initiate_upload_response |
                         (unused << unused_byte_shift)));
        std::copy(read.data.begin(), read.data.end(),
                  response.data.begin() + data_offset);
        return SdoServerResult{response, {}};
    }

    if ((command & expedited_download_mask) == expedited_download_pattern) {
        const std::uint8_t unused = static_cast<std::uint8_t>(
            (command >> unused_byte_shift) & unused_byte_mask);
        const std::size_t size = scalar_capacity - unused;
        const DictionaryWriteResult write = dictionary_.write(
            index, subindex, BytesView(request.data.data() + data_offset, size));
        if (write.abort != SdoAbort::none) {
            return abort_response(request, write.abort);
        }
        return SdoServerResult{
            response_header(request, initiate_download_response),
            write.effects};
    }

    return abort_response(request, SdoAbort::command_specifier);
}

SdoServerResult CanopenSdoServer::abort_response(const CanFrame& request,
                                                 SdoAbort abort) {
    CanFrame response = response_header(request, abort_transfer_response);
    const std::uint32_t code = static_cast<std::uint32_t>(abort);
    for (std::size_t offset = 0U; offset < scalar_capacity; ++offset) {
        response.data[data_offset + offset] = static_cast<std::uint8_t>(
            code >> (bits_per_byte * offset));
    }
    return {response, {}};
}

CanFrame CanopenSdoServer::response_header(const CanFrame& request,
                                           std::uint8_t command) {
    CanFrame response;
    response.identifier = canopen::sdo_response_identifier;
    response.size = complete_sdo_frame_size;
    response.data[command_offset] = command;
    response.data[index_low_offset] = request.data[index_low_offset];
    response.data[index_high_offset] = request.data[index_high_offset];
    response.data[subindex_offset] = request.data[subindex_offset];
    return response;
}

}  // namespace firmware::core
