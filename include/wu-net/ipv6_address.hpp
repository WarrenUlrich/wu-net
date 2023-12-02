#pragma once

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace net {
class ipv6_address {
public:
  std::array<std::uint16_t, 8> groups;

  explicit constexpr ipv6_address(
      const std::array<std::uint16_t, 8> &groups)
      : groups(groups) {}

  constexpr ipv6_address(std::uint16_t g1, std::uint16_t g2,
                         std::uint16_t g3, std::uint16_t g4,
                         std::uint16_t g5, std::uint16_t g6,
                         std::uint16_t g7, std::uint16_t g8)
      : groups({g1, g2, g3, g4, g5, g6, g7, g8}) {}

  std::string to_string() const noexcept {
    std::stringstream ss;
    for (size_t i = 0; i < groups.size(); ++i) {
      ss << std::hex << groups[i];
      if (i < groups.size() - 1) {
        ss << ":";
      }
    }
    return ss.str();
  }
};
} // namespace net
