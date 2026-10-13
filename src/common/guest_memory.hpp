#pragma once
// FHNX: portable big-endian guest memory access (does NOT execute Forza code).
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <span>

namespace fhnx {
class GuestMemory {
public:
  GuestMemory(std::span<std::uint8_t> bytes, std::uint32_t base)
      : bytes_(bytes), base_(base) {}

  std::uint16_t load16(std::uint32_t address) const {
    const auto i = offset(address, 2);
    return static_cast<std::uint16_t>((std::uint16_t(bytes_[i]) << 8) |
                                      std::uint16_t(bytes_[i + 1]));
  }
  std::uint32_t load32(std::uint32_t address) const {
    const auto i = offset(address, 4);
    return (std::uint32_t(bytes_[i]) << 24) |
           (std::uint32_t(bytes_[i + 1]) << 16) |
           (std::uint32_t(bytes_[i + 2]) << 8) |
           std::uint32_t(bytes_[i + 3]);
  }
  std::uint64_t load64(std::uint32_t address) const {
    const auto i = offset(address, 8);
    std::uint64_t result = 0;
    for (std::size_t n = 0; n != 8; ++n) result = (result << 8) | bytes_[i + n];
    return result;
  }
  void store16(std::uint32_t address, std::uint16_t value) {
    const auto i = offset(address, 2);
    bytes_[i] = static_cast<std::uint8_t>(value >> 8);
    bytes_[i + 1] = static_cast<std::uint8_t>(value);
  }
  void store32(std::uint32_t address, std::uint32_t value) {
    const auto i = offset(address, 4);
    for (int n = 0; n != 4; ++n)
      bytes_[i + static_cast<std::size_t>(n)] = static_cast<std::uint8_t>(value >> (24 - n * 8));
  }
  void store64(std::uint32_t address, std::uint64_t value) {
    const auto i = offset(address, 8);
    for (int n = 0; n != 8; ++n)
      bytes_[i + static_cast<std::size_t>(n)] = static_cast<std::uint8_t>(value >> (56 - n * 8));
  }

private:
  std::size_t offset(std::uint32_t address, std::size_t count) const {
    // Check in 64-bit space to reject wraparound and truncated addresses.
    if (address < base_) throw std::out_of_range("guest address precedes base");
    const std::uint64_t off = std::uint64_t(address) - base_;
    if (off > bytes_.size() || count > bytes_.size() - off)
      throw std::out_of_range("guest access outside mapped region");
    return static_cast<std::size_t>(off);
  }
  std::span<std::uint8_t> bytes_;
  std::uint32_t base_;
};
}  // namespace fhnx
