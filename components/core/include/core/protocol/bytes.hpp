/** @file @brief Owning byte storage and a lightweight read-only protocol view. */
#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>
namespace firmware::core {
/// Contiguous owning storage used at protocol and adapter boundaries.
using ByteVector = std::vector<std::uint8_t>;

/** Non-owning immutable byte range that avoids copying protocol input.
 *  The caller must keep the referenced storage alive for the lifetime of the
 *  view. Like `std::span`, indexed access is intentionally unchecked.
 */
class BytesView {
public:
    /// Creates an empty view.
    constexpr BytesView() = default;
    /// Views @p size bytes beginning at @p data without taking ownership.
    constexpr BytesView(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}
    /// Views an owning byte vector for the duration of that vector's validity.
    BytesView(const ByteVector& bytes) : data_(bytes.data()), size_(bytes.size()) {}
    /// Views the object representation of a string without copying it.
    BytesView(std::string_view text) : data_(reinterpret_cast<const std::uint8_t*>(text.data())), size_(text.size()) {}
    /// Returns the first byte pointer, which may be null for an empty view.
    constexpr const std::uint8_t* data() const {
        return data_;
    }

    /// Returns the number of readable bytes.
    constexpr std::size_t size() const {
        return size_;
    }

    /// Returns an iterator to the first byte.
    constexpr const std::uint8_t* begin() const {
        return data_;
    }

    /// Returns the one-past-last byte iterator.
    constexpr const std::uint8_t* end() const {
        return data_ + size_;
    }

    /// Returns one byte; callers must ensure @p index is smaller than size().
    constexpr std::uint8_t operator[](std::size_t index) const {
        return data_[index];
    }
private:
    const std::uint8_t* data_ = nullptr;
    std::size_t size_ = 0;
};
}  // namespace firmware::core
