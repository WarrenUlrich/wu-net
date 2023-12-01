#pragma once

#include <array>
#include <cstdint>

namespace net {
class ipv4_address {
public:
  std::array<std::uint8_t, 4> octets;
};
} // namespace net