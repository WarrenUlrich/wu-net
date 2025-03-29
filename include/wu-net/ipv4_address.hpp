#pragma once

#include <array>
#include <cstdint>
#include <format>
#include <string>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace net {
class ipv4_address {
public:
  constexpr ipv4_address() : _octets({0, 0, 0, 0}) {}

  constexpr ipv4_address(std::array<std::uint8_t, 4> octets)
      : _octets(octets) {}

  constexpr ipv4_address(std::uint8_t a, std::uint8_t b, std::uint8_t c,
                         std::uint8_t d)
      : _octets({a, b, c, d}) {}

  constexpr ipv4_address(const ipv4_address &) = default;

  constexpr ipv4_address(ipv4_address &&) = default;

  ipv4_address &operator=(const ipv4_address &) = default;

  ipv4_address &operator=(ipv4_address &&) = default;

  std::string to_string() const {
    return std::format("{}.{}.{}.{}", _octets[0], _octets[1], _octets[2],
                       _octets[3]);
  }

  const std::array<std::uint8_t, 4> &octets() const { return _octets; }

  template <std::ranges::contiguous_range Range>
    requires std::is_same_v<std::ranges::range_value_t<Range>, char>
  static std::optional<ipv4_address> from_str(const Range &str) {
    const char *start = std::ranges::data(str);
    const char *end = start + std::ranges::size(str);

    if (start < end && *(end - 1) == '\0') {
      --end;
    }

    // Empty string check
    if (start == end) {
      return std::nullopt;
    }

    std::array<std::uint8_t, 4> octets{};
    const char *current = start;

    for (size_t i = 0; i < 4; ++i) {
      // Check if we have data for this octet
      if (current >= end) {
        return std::nullopt;
      }

      // Parse the octet
      unsigned int value = 0;
      auto result = std::from_chars(current, end, value);

      if (result.ec != std::errc{} || value > 255) {
        return std::nullopt;
      }

      octets[i] = static_cast<std::uint8_t>(value);
      current = result.ptr;

      if (i < 3) {
        if (current >= end || *current != '.') {
          return std::nullopt;
        }
        ++current;
      }
    }

    if (current != end) {
      return std::nullopt;
    }

    return ipv4_address(octets);
  }

  static std::optional<ipv4_address> resolve_host(std::string_view hostname) {
    struct addrinfo hints = {}, *results = nullptr;
    hints.ai_family = AF_INET;       // Only IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP

    // Get address info
    int gai_res =
        getaddrinfo(std::string(hostname).c_str(), nullptr, &hints, &results);
    if (gai_res != 0) {
      return std::nullopt;
    }

    struct _cleaner {
      addrinfo *ptr;
      ~_cleaner() {
        if (ptr)
          freeaddrinfo(ptr);
      }
    } cleaner{results};

    for (struct addrinfo *addr = results; addr != nullptr;
         addr = addr->ai_next) {
      if (addr->ai_family == AF_INET) {
        struct sockaddr_in *ipv4 =
            reinterpret_cast<struct sockaddr_in *>(addr->ai_addr);

        uint32_t ip_addr = ntohl(ipv4->sin_addr.s_addr);
        return ipv4_address(static_cast<uint8_t>((ip_addr >> 24) & 0xFF),
                            static_cast<uint8_t>((ip_addr >> 16) & 0xFF),
                            static_cast<uint8_t>((ip_addr >> 8) & 0xFF),
                            static_cast<uint8_t>(ip_addr & 0xFF));
      }
    }

    return std::nullopt;
  }

  static const ipv4_address loopback;  // 127.0.0.1
  static const ipv4_address any;       // 0.0.0.0
  static const ipv4_address broadcast; // 255.255.255.255

private:
  std::array<std::uint8_t, 4> _octets;
};

const ipv4_address ipv4_address::loopback = ipv4_address(127, 0, 0, 1);
const ipv4_address ipv4_address::any = ipv4_address(0, 0, 0, 0);
const ipv4_address ipv4_address::broadcast = ipv4_address(255, 255, 255, 255);

} // namespace net

template <> struct std::formatter<net::ipv4_address> {
  constexpr auto parse(std::format_parse_context &ctx) { return ctx.begin(); }

  auto format(const net::ipv4_address &addr, std::format_context &ctx) const {
    const auto &octets = addr.octets();
    return std::format_to(ctx.out(), "{}.{}.{}.{}", octets[0], octets[1],
                          octets[2], octets[3]);
  }
};
