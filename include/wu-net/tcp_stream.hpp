#pragma once

#include <arpa/inet.h>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <optional>
#include <string_view>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

namespace net {
class tcp_streambuf : public std::streambuf {
public:
  explicit tcp_streambuf(int sock_fd) : _sock_fd(sock_fd) {
    setg(_input_buffer, _input_buffer, _input_buffer);
    setp(_output_buffer,
         _output_buffer + _output_buffer_size - 1);
  }

protected:
  int_type underflow() override {
    if (gptr() < egptr()) { // buffer not exhausted
      return traits_type::to_int_type(*gptr());
    }

    int num = recv(_sock_fd, _input_buffer,
                   _input_buffer_size, 0);
    if (num <= 0) { // error or connection closed
      return traits_type::eof();
    }

    setg(_input_buffer, _input_buffer, _input_buffer + num);
    return traits_type::to_int_type(*gptr());
  }

  int_type overflow(int_type ch) override {
    if (ch != traits_type::eof()) {
      *pptr() = ch;
      pbump(1);
    }

    if (sync() == 0) {
      return ch;
    } else {
      return traits_type::eof();
    }
  }

  int sync() override {
    int num = pptr() - pbase();
    if (send(_sock_fd, _output_buffer, num, 0) != num) {
      return -1; // error
    }

    pbump(-num); // reset put pointer
    return 0;
  }

private:
  static constexpr size_t _input_buffer_size = 1024;
  static constexpr size_t _output_buffer_size = 1024;
  char _input_buffer[_input_buffer_size];
  char _output_buffer[_output_buffer_size];
  int _sock_fd;
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

  static std::optional<tcp_stream>
  connect(std::string_view addr) noexcept;

private:
  int _sock_fd;
  tcp_streambuf _streambuf;
};

std::optional<tcp_stream>
tcp_stream::connect(std::string_view addr) noexcept {
  size_t colon_pos = addr.find(':');
  if (colon_pos == std::string_view::npos) {
    return std::nullopt;
  }

  std::string hostname =
      std::string(addr.substr(0, colon_pos));
  std::string port =
      std::string(addr.substr(colon_pos + 1));

  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
  hints.ai_socktype = SOCK_STREAM;

  if (getaddrinfo(hostname.c_str(), port.c_str(), &hints,
                  &res) != 0) {
    return std::nullopt;
  }

  int sockfd = socket(res->ai_family, res->ai_socktype,
                      res->ai_protocol);
  if (sockfd < 0) {
    freeaddrinfo(res);
    return std::nullopt;
  }

  if (::connect(sockfd, res->ai_addr, res->ai_addrlen) <
      0) {
    close(sockfd);
    freeaddrinfo(res);
    return std::nullopt;
  }

  freeaddrinfo(res);
  return tcp_stream(sockfd);
}
} // namespace net