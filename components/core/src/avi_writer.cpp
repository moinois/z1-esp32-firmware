// Implements little-endian 224-byte-header MJPEG AVI segment generation.
#include "firmware/core/avi_writer.hpp"

#include <algorithm>

namespace firmware::core {
namespace {

void put_u32(ByteVector& data, std::size_t offset, std::uint32_t value) {
    for (unsigned bit = 0U; bit < 32U; bit += 8U) data[offset + bit / 8U] = static_cast<std::uint8_t>(value >> bit);
}

void put_fourcc(ByteVector& data, std::size_t offset, const char* value) {
    for (std::size_t i = 0U; i < 4U; ++i) data[offset + i] = static_cast<std::uint8_t>(value[i]);
}

}  // namespace

AviWriter::AviWriter(std::uint32_t width, std::uint32_t height)
    : width_(width), height_(height), data_(224U, 0U) {
    put_fourcc(data_, 0U, "RIFF");
    put_fourcc(data_, 8U, "AVI ");
    put_fourcc(data_, 12U, "LIST");
    put_u32(data_, 16U, 0xc0U);
    put_fourcc(data_, 20U, "hdrl");
    put_fourcc(data_, 24U, "avih");
    put_u32(data_, 28U, 0x38U);
    put_u32(data_, 32U, 100000U);
    put_u32(data_, 40U, 0x800U);
    put_u32(data_, 44U, 0x810U);
    put_u32(data_, 56U, 1U);
    put_u32(data_, 60U, 0x800U);
    put_u32(data_, 64U, width_);
    put_u32(data_, 68U, height_);
    put_fourcc(data_, 88U, "LIST");
    put_u32(data_, 92U, 0x74U);
    put_fourcc(data_, 96U, "strl");
    put_fourcc(data_, 100U, "strh");
    put_u32(data_, 104U, 0x38U);
    put_fourcc(data_, 108U, "vids");
    put_fourcc(data_, 112U, "MJPG");
    put_u32(data_, 128U, 1U);
    put_u32(data_, 132U, 10U);
    put_u32(data_, 148U, 0x0cU);
    put_fourcc(data_, 164U, "strf");
    put_u32(data_, 168U, 0x28U);
    put_u32(data_, 172U, 0x28U);
    put_u32(data_, 176U, width_);
    put_u32(data_, 180U, height_);
    data_[184U] = 1U;
    data_[186U] = 24U;
    put_fourcc(data_, 188U, "MJPG");
    put_u32(data_, 192U, width_ * height_ * 3U);
    put_fourcc(data_, 212U, "LIST");
    put_fourcc(data_, 220U, "movi");
}

bool AviWriter::append_frame(BytesView jpeg) {
    if (finalized_ || jpeg.size() > 0xffffffffU) return false;
    const std::size_t offset = data_.size() - 220U;
    entries_.push_back({static_cast<std::uint32_t>(offset), static_cast<std::uint32_t>(jpeg.size())});
    data_.insert(data_.end(), {'0', '0', 'd', 'c'});
    const auto old_size = data_.size();
    data_.resize(old_size + 4U + jpeg.size());
    put_u32(data_, old_size, static_cast<std::uint32_t>(jpeg.size()));
    std::copy(jpeg.begin(), jpeg.end(), data_.begin() + old_size + 4U);
    const std::size_t padding = 2048U - (jpeg.size() % 2048U);
    data_.insert(data_.end(), padding, 0U);
    return true;
}

std::optional<ByteVector> AviWriter::finalize() {
    if (finalized_) return std::nullopt;
    finalized_ = true;
    put_u32(data_, 48U, static_cast<std::uint32_t>(entries_.size()));
    put_u32(data_, 140U, static_cast<std::uint32_t>(entries_.size()));
    put_u32(data_, 216U, static_cast<std::uint32_t>(data_.size() - 224U));
    const std::size_t index_offset = data_.size();
    data_.resize(index_offset + 8U + entries_.size() * 16U, 0U);
    put_fourcc(data_, index_offset, "idx1");
    put_u32(data_, index_offset + 4U, static_cast<std::uint32_t>(entries_.size() * 16U));
    for (std::size_t i = 0U; i < entries_.size(); ++i) {
        const std::size_t offset = index_offset + 8U + i * 16U;
        put_fourcc(data_, offset, "00dc");
        put_u32(data_, offset + 4U, 0x10U);
        put_u32(data_, offset + 8U, entries_[i].offset);
        put_u32(data_, offset + 12U, entries_[i].size);
    }
    put_u32(data_, 4U, static_cast<std::uint32_t>(data_.size() - 8U));
    return data_;
}

}  // namespace firmware::core
