#pragma once

#include <arpa/inet.h>
#include <cstring>
#include <expected>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <optional>
#include <string_view>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>
#include <poll.h>

namespace net {
enum class tcp_stream_error { invalid_address_format };

class tcp_stream_error_category
    : public std::error_category {
public:
  const char *name() const noexcept override {
    return "tcp_stream_error";
  }

  std::string message(int ev) const override {
    switch (static_cast<tcp_stream_error>(ev)) {
    case tcp_stream_error::invalid_address_format:
      return "Invalid address format";
    default:
      return "Unknown error";
    }
  }
};

const std::error_category &
get_tcp_stream_error_category() noexcept {
  static tcp_stream_error_category instance;
  return instance;
}

std::error_code
make_error_code(tcp_stream_error e) noexcept {
  return {static_cast<int>(e),
          get_tcp_stream_error_category()};
}

class tcp_streambuf : public std::streambuf {
public:
  explicit tcp_streambuf(int sock_fd)
      : _sock_fd(sock_fd), _input_buffer(1024),
        _output_buffer(1024) {
    setg(_input_buffer.data(),
         _input_buffer.data() + _input_buffer.size(),
         _input_buffer.data() + _input_buffer.size());

    setp(_output_buffer.data(),
         _output_buffer.data() + _output_buffer.size());
  }

protected:
  int_type underflow() override {
    if (gptr() < egptr()) { // buffer not exhausted
      return traits_type::to_int_type(*gptr());
    }

    int num = read(_sock_fd, _input_buffer.data(),
                   _input_buffer.size());
    if (num <= 0) {
      return traits_type::eof();
    }

    setg(_input_buffer.data(), _input_buffer.data(),
         _input_buffer.data() + num);

    return traits_type::to_int_type(*gptr());
  }

  // Override overflow for output
  int_type
  overflow(int_type ch = traits_type::eof()) override {
    if (!traits_type::eq_int_type(ch, traits_type::eof())) {
      *pptr() = traits_type::to_char_type(ch);
      pbump(1);
    }

    if (sync() == 0) {
      return ch;
    } else {
      return traits_type::eof();
    }
  }

  // Override sync for output
  int sync() override {
    int num = pptr() - pbase();
    if (write(_sock_fd, _output_buffer.data(), num) !=
        num) {
      return -1;
    }

    pbump(-num); // reset put pointer
    return 0;
  }

private:
  int _sock_fd;
  std::vector<char> _input_buffer;
  std::vector<char> _output_buffer;
};

class tcp_stream : public std::iostream {
public:
  tcp_stream(const tcp_stream &) = delete;

  tcp_stream &operator=(const tcp_stream &) = delete;

  tcp_stream(tcp_stream &&other) noexcept
      : std::iostream(nullptr),
        _sock_fd(std::exchange(other._sock_fd, -1)),
        _streambuf(_sock_fd) {
    rdbuf(&_streambuf);
  }

  explicit tcp_stream(int sock_fd)
      : std::iostream(nullptr), _sock_fd(sock_fd),
        _streambuf(_sock_fd) {
    rdbuf(&_streambuf);
  }

  tcp_stream &operator=(tcp_stream &&other) noexcept {
    if (this != &other) {
      close(_sock_fd);
      _sock_fd = std::exchange(other._sock_fd, -1);
      _streambuf = std::move(other._streambuf);
    }
    return *this;
  }

  bool set_nodelay(bool enabled) noexcept {
    int flag = enabled ? 0 : 1;
    if (setsockopt(_sock_fd, IPPROTO_TCP, TCP_NODELAY,
                   (char *)&flag, sizeof(int)) < 0) {
      return false;
    }

    return true;
  }

  bool set_keep_alive(bool enabled, int idle_time = 30,
                      int interval = 10,
                      int max_probes = 5) noexcept {
    int optval = enabled ? 1 : 0;
    if (setsockopt(_sock_fd, SOL_SOCKET, SO_KEEPALIVE,
                   &optval, sizeof(optval)) < 0) {
      return false;
    }

    if (enabled) {
      // Set the idle time before starting keep-alive probes
      if (setsockopt(_sock_fd, IPPROTO_TCP, TCP_KEEPIDLE,
                     &idle_time, sizeof(idle_time)) < 0) {
        return false;
      }

      // Set the interval between keep-alive probes
      if (setsockopt(_sock_fd, IPPROTO_TCP, TCP_KEEPINTVL,
                     &interval, sizeof(interval)) < 0) {
        return false;
      }

      // Set the max number of keep-alive probes
      if (setsockopt(_sock_fd, IPPROTO_TCP, TCP_KEEPCNT,
                     &max_probes, sizeof(max_probes)) < 0) {
        return false;
      }
    }

    return true;
  }

  bool set_nonblocking(bool enabled) {
    int flags = fcntl(_sock_fd, F_GETFL, 0);
    if (flags < 0) {
      return false;
    }

    if (enabled) {
      flags |= O_NONBLOCK;
    } else {
      flags &= ~O_NONBLOCK;
    }

    if (fcntl(_sock_fd, F_SETFL, flags) != 0) {
      return false;
    }

    return true;
  }

  bool is_ready_for_read(int timeout_ms) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(_sock_fd, &read_fds);

    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    int result = select(_sock_fd + 1, &read_fds, nullptr,
                        nullptr, &timeout);
    if (result < 0) {
      // Handle error
      return false;
    }

    return FD_ISSET(_sock_fd, &read_fds);
  }

  bool ready_for_write(int timeout_ms) {
        struct pollfd fds[1];
        fds[0].fd = _sock_fd;
        fds[0].events = POLLOUT;

        int ret = poll(fds, 1, timeout_ms);
        if (ret < 0) {
            // Error occurred
            return false;
        } 
        if (ret == 0) {
            // Timeout
            return false;
        } 
        if (fds[0].revents & POLLOUT) {
            // Socket is ready for write
            return true;
        }

        // Other cases, like error on socket
        return false;
    }

  static std::expected<tcp_stream, std::error_code>
  connect(std::string_view addr) noexcept;

private:
  int _sock_fd;
  tcp_streambuf _streambuf;
};

std::expected<tcp_stream, std::error_code>
tcp_stream::connect(std::string_view addr_str) noexcept {
  size_t colon_pos = addr_str.find(':');
  if (colon_pos == std::string_view::npos) {
    return std::unexpected(make_error_code(
        tcp_stream_error::invalid_address_format));
  }

  std::string hostname =
      std::string(addr_str.substr(0, colon_pos));

  std::string port =
      std::string(addr_str.substr(colon_pos + 1));

  struct addrinfo addr_hints, *addr;
  memset(&addr_hints, 0, sizeof(addr_hints));
  addr_hints.ai_family = AF_UNSPEC;
  addr_hints.ai_socktype = SOCK_STREAM;

  if (auto result =
          getaddrinfo(hostname.c_str(), port.c_str(),
                      &addr_hints, &addr);
      result != 0) {
    return std::unexpected(
        std::error_code(result, std::system_category()));
  }

  int sock_fd = socket(addr->ai_family, addr->ai_socktype,
                       addr->ai_protocol);
  if (sock_fd < 0) {
    int err = errno;
    freeaddrinfo(addr);
    return std::unexpected(
        std::error_code(err, std::generic_category()));
  }

  if (::connect(sock_fd, addr->ai_addr, addr->ai_addrlen) <
      0) {
    int err = errno;
    close(sock_fd);
    freeaddrinfo(addr);
    return std::unexpected(
        std::error_code(err, std::system_category()));
  }

  return tcp_stream(sock_fd);
}

} // namespace net