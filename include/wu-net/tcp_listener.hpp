#pragma once

#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <generator>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "tcp_stream.hpp"

namespace net {

// tcp_listener class for accepting TCP connections
class tcp_listener {
public:
  // Default constructor
  tcp_listener() : socket_fd_(-1) {}

  // No copy
  tcp_listener(const tcp_listener &) = delete;
  tcp_listener &operator=(const tcp_listener &) = delete;

  // Move
  tcp_listener(tcp_listener &&other) noexcept
      : socket_fd_(std::exchange(other.socket_fd_, -1)) {}

  tcp_listener &operator=(tcp_listener &&other) noexcept {
    if (this != &other) {
      close();
      socket_fd_ = std::exchange(other.socket_fd_, -1);
    }
    return *this;
  }

  // Destructor
  ~tcp_listener() { close(); }

  // Check if the listener is open
  bool is_open() const noexcept { return socket_fd_ >= 0; }

  // Close the listener
  void close() {
    if (is_open()) {
      ::close(socket_fd_);
      socket_fd_ = -1;
    }
  }

  // Accept a new connection (blocking)
  std::optional<tcp_stream> accept() {
    if (!is_open()) {
      return std::nullopt;
    }

    // Accept a connection
    int client_fd = ::accept(socket_fd_, nullptr, nullptr);
    if (client_fd < 0) {
      return std::nullopt;
    }

    return tcp_stream(client_fd);
  }

  std::generator<tcp_stream> connections(const bool *should_stop = nullptr,
                                         int timeout_ms = 100) {
    while (is_open() && (should_stop == nullptr | !*should_stop)) {
      auto client = accept();
      if (client) {
        co_yield std::move(*client);
      }
    }
  }

  // Accept with timeout (in milliseconds, -1 for infinite)
  std::optional<tcp_stream> accept(int timeout_ms) {
    if (!is_open()) {
      return std::nullopt;
    }

    // Set up for poll
    struct pollfd pfd;
    pfd.fd = socket_fd_;
    pfd.events = POLLIN;
    pfd.revents = 0;

    // Wait for a connection
    int poll_result = poll(&pfd, 1, timeout_ms);

    if (poll_result <= 0) {
      return std::nullopt; // Timeout or error
    }

    if (!(pfd.revents & POLLIN)) {
      return std::nullopt; // Not ready for reading
    }

    // Accept a connection
    int client_fd = ::accept(socket_fd_, nullptr, nullptr);
    if (client_fd < 0) {
      return std::nullopt;
    }

    return tcp_stream(client_fd);
  }

  // Get the socket file descriptor
  int native_handle() const noexcept { return socket_fd_; }

  // Socket options

  // Set SO_REUSEADDR
  bool set_reuseaddr(bool enable) noexcept {
    int value = enable ? 1 : 0;
    return setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &value,
                      sizeof(value)) == 0;
  }

  // Set SO_REUSEPORT (if available)
  bool set_reuseport(bool enable) noexcept {
#ifdef SO_REUSEPORT
    int value = enable ? 1 : 0;
    return setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEPORT, &value,
                      sizeof(value)) == 0;
#else
    return false;
#endif
  }

  // Set non-blocking mode
  bool set_nonblocking(bool enable) noexcept {
    int flags = fcntl(socket_fd_, F_GETFL, 0);
    if (flags < 0)
      return false;

    if (enable)
      flags |= O_NONBLOCK;
    else
      flags &= ~O_NONBLOCK;

    return fcntl(socket_fd_, F_SETFL, flags) == 0;
  }

  // Get the local address
  std::optional<std::string> local_address() const {
    if (!is_open()) {
      return std::nullopt;
    }

    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);

    if (getsockname(socket_fd_, (struct sockaddr *)&addr, &addr_len) < 0) {
      return std::nullopt;
    }

    char host[NI_MAXHOST], service[NI_MAXSERV];
    int res =
        getnameinfo((struct sockaddr *)&addr, addr_len, host, sizeof(host),
                    service, sizeof(service), NI_NUMERICHOST | NI_NUMERICSERV);

    if (res != 0) {
      return std::nullopt;
    }

    return std::string(host) + ":" + service;
  }

  // Static factory method
  static std::optional<tcp_listener> create(std::string_view address,
                                            int backlog = 5) {
    // Parse host:port format
    size_t colon_pos = address.find(':');
    if (colon_pos == std::string_view::npos) {
      return std::nullopt;
    }

    std::string host(address.substr(0, colon_pos));
    std::string port(address.substr(colon_pos + 1));

    // Handle special case for wildcard address
    if (host == "*") {
      host = ""; // Let getaddrinfo handle it with AI_PASSIVE
    }

    // Set up address hints
    struct addrinfo hints = {}, *results = nullptr;
    hints.ai_family = AF_UNSPEC;     // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;     // For wildcard IP address

    // Get address info
    int gai_res = getaddrinfo(host.empty() ? nullptr : host.c_str(),
                              port.c_str(), &hints, &results);
    if (gai_res != 0) {
      return std::nullopt;
    }

    // Cleanup helper
    struct _cleaner {
      addrinfo *ptr;
      ~_cleaner() {
        if (ptr)
          freeaddrinfo(ptr);
      }
    } cleaner{results};

    tcp_listener listener;

    // Try each address until we succeed
    for (struct addrinfo *addr = results; addr != nullptr;
         addr = addr->ai_next) {
      // Create socket
      int sock_fd =
          socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
      if (sock_fd < 0) {
        continue;
      }

      // Set SO_REUSEADDR by default
      int reuse = 1;
      if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
          0) {
        ::close(sock_fd);
        continue;
      }

      // Bind to the address
      if (bind(sock_fd, addr->ai_addr, addr->ai_addrlen) < 0) {
        ::close(sock_fd);
        continue;
      }

      // Start listening
      if (listen(sock_fd, backlog) < 0) {
        ::close(sock_fd);
        continue;
      }

      listener.socket_fd_ = sock_fd;
      break; // Successfully bound and listening
    }

    if (!listener.is_open()) {
      return std::nullopt;
    }

    return listener;
  }

private:
  int socket_fd_;
};

} // namespace net