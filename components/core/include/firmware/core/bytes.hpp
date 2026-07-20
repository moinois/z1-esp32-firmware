// Defines lightweight byte containers and non-owning views for protocol code.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>
namespace firmware::core {
using ByteVector = std::vector<std::uint8_t>;
class BytesView {
public:
    constexpr BytesView() = default;
    constexpr BytesView(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}
    BytesView(const ByteVector& bytes) : data_(bytes.data()), size_(bytes.size()) {}
    BytesView(std::string_view text) : data_(reinterpret_cast<const std::uint8_t*>(text.data())), size_(text.size()) {}
    constexpr const std::uint8_t* data() const {
        return data_;
    }

    constexpr std::size_t size() const {
        return size_;
    }

    constexpr const std::uint8_t* begin() const {
        return data_;
    }

    constexpr const std::uint8_t* end() const {
        return data_ + size_;
    }

    constexpr std::uint8_t operator[](std::size_t index) const {
        return data_[index];
    }
private:
    const std::uint8_t* data_ = nullptr;
    std::size_t size_ = 0;
};
}  // namespace firmware::core
