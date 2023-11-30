#pragma once

#include <arpa/inet.h>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <optional>
#include <string_view>
#include <sys/socket.h>
#include <unistd.h>

#include "tcp_stream.hpp"

namespace net {
class tcp_listener {
public:
  tcp_listener(const tcp_listener &) = delete;

  tcp_listener(tcp_listener &&other) noexcept
      : _listen_fd(std::exchange(other._listen_fd, -1)) {}

  explicit tcp_listener(int listen_fd)
      : _listen_fd(listen_fd) {}

  tcp_listener &operator=(const tcp_listener &) = delete;

  tcp_listener &operator=(tcp_listener &&other) noexcept {
    if (this != &other) {
      close(_listen_fd);
      _listen_fd = std::exchange(other._listen_fd, -1);
    }
    return *this;
  }

  ~tcp_listener() {
    if (_listen_fd >= 0) {
      close(_listen_fd);
    }
  }

  std::optional<tcp_stream> accept() const noexcept {
    int client_fd = ::accept(_listen_fd, nullptr, nullptr);
    if (client_fd < 0) {
      return std::nullopt;
    }

    return tcp_stream(client_fd);
  }

  static std::optional<tcp_listener>
  bind(std::string_view addr) noexcept {
    size_t colon_pos = addr.find(':');
    if (colon_pos == std::string_view::npos) {
      return std::nullopt;
    }

    std::string hostname =
        std::string(addr.substr(0, colon_pos));
    std::string port =
        std::string(addr.substr(colon_pos + 1));

    struct addrinfo hints, *res;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(hostname.c_str(), port.c_str(), &hints,
                    &res) != 0) {
      return std::nullopt;
    }

    int listen_fd = socket(res->ai_family, res->ai_socktype,
                           res->ai_protocol);
    if (listen_fd < 0) {
      freeaddrinfo(res);
      return std::nullopt;
    }

    if (::bind(listen_fd, res->ai_addr, res->ai_addrlen) <
        0) {
      close(listen_fd);
      freeaddrinfo(res);
      return std::nullopt;
    }

    freeaddrinfo(res);

    if (listen(listen_fd, 10) <
        0) { // setting backlog to 10
      close(listen_fd);
      return std::nullopt;
    }

    return tcp_listener(listen_fd);
  }

private:
  int _listen_fd;
};
} // namespace net