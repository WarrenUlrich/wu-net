#pragma once

#include <array>
#include <cstdint>
#include <sstream>
#include <string>

namespace net {
class ipv4_address {
public:
  std::array<std::uint8_t, 4> octets;

  explicit constexpr ipv4_address(
      const std::array<std::uint8_t, 4> octets)
      : octets(octets) {}

  constexpr ipv4_address(std::uint8_t o1, std::uint8_t o2,
                         std::uint8_t o3, std::uint8_t o4)
      : octets({o1, o2, o3, o4}) {}

  std::string to_string() const noexcept {
    std::stringstream ss;
    for (size_t i = 0; i < octets.size(); ++i) {
      ss << static_cast<int>(octets[i]);
      if (i < octets.size() - 1) {
        ss << ".";
      }
    }
    return ss.str();
  }
};
} // namespace net