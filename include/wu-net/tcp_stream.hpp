#pragma once

#include <iostream>
#include <optional>
#include <streambuf>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace net {

// Custom error codes for tcp operations
enum class tcp_error {
  invalid_address_format = 1,
  connection_failed,
  not_connected
};

// Define error category for tcp errors
class tcp_error_category : public std::error_category {
public:
  const char *name() const noexcept override { return "tcp_stream"; }

  std::string message(int ev) const override {
    switch (static_cast<tcp_error>(ev)) {
    case tcp_error::invalid_address_format:
      return "Invalid address format";
    case tcp_error::connection_failed:
      return "Connection failed";
    case tcp_error::not_connected:
      return "Socket not connected";
    default:
      return "Unknown tcp error";
    }
  }
};

// Get singleton instance of error category
const std::error_category &tcp_category() noexcept {
  static const tcp_error_category instance;
  return instance;
}

// Helper function to create error codes
inline std::error_code make_error_code(tcp_error e) noexcept {
  return {static_cast<int>(e), tcp_category()};
}

// Streambuf implementation for TCP sockets
class tcp_streambuf : public std::streambuf {
public:
  explicit tcp_streambuf(int socket_fd, std::size_t buffer_size = 4096)
      : socket_fd_(socket_fd), input_buffer_(buffer_size),
        output_buffer_(buffer_size) {

    // Initialize the get area pointers (empty buffer)
    char *end = input_buffer_.data() + input_buffer_.size();
    setg(end, end, end);

    // Initialize the put area
    setp(output_buffer_.data(), output_buffer_.data() + output_buffer_.size());
  }

  // No copy
  tcp_streambuf(const tcp_streambuf &) = delete;
  tcp_streambuf &operator=(const tcp_streambuf &) = delete;

  // Move constructor
  tcp_streambuf(tcp_streambuf &&other) noexcept
      : socket_fd_(std::exchange(other.socket_fd_, -1)),
        input_buffer_(std::move(other.input_buffer_)),
        output_buffer_(std::move(other.output_buffer_)) {

    // Preserve get area state
    if (other.gptr()) {
      setg(input_buffer_.data(),
           input_buffer_.data() + (other.gptr() - other.eback()),
           input_buffer_.data() + (other.egptr() - other.eback()));
    }

    // Preserve put area state
    if (other.pptr()) {
      setp(output_buffer_.data(),
           output_buffer_.data() + output_buffer_.size());
      pbump(other.pptr() - other.pbase());
    }
  }

  // Move assignment
  tcp_streambuf &operator=(tcp_streambuf &&other) noexcept {
    if (this != &other) {
      socket_fd_ = std::exchange(other.socket_fd_, -1);
      input_buffer_ = std::move(other.input_buffer_);
      output_buffer_ = std::move(other.output_buffer_);

      // Preserve get area state
      if (other.gptr()) {
        setg(input_buffer_.data(),
             input_buffer_.data() + (other.gptr() - other.eback()),
             input_buffer_.data() + (other.egptr() - other.eback()));
      }

      // Preserve put area state
      if (other.pptr()) {
        setp(output_buffer_.data(),
             output_buffer_.data() + output_buffer_.size());
        pbump(other.pptr() - other.pbase());
      }
    }
    return *this;
  }

  ~tcp_streambuf() {
    sync(); // Flush any pending data
  }

  bool is_open() const noexcept { return socket_fd_ >= 0; }

protected:
  // Called when we need more data for reading
  int_type underflow() override {
    // Return the current character if there's still data in the buffer
    if (gptr() < egptr())
      return traits_type::to_int_type(*gptr());

    // Read new data
    ssize_t bytes_read =
        read(socket_fd_, input_buffer_.data(), input_buffer_.size());

    // Handle errors or EOF
    if (bytes_read <= 0)
      return traits_type::eof();

    // Update get area pointers
    setg(input_buffer_.data(), input_buffer_.data(),
         input_buffer_.data() + bytes_read);

    // Return the first character
    return traits_type::to_int_type(*gptr());
  }

  // Called when the put buffer is full
  int_type overflow(int_type ch = traits_type::eof()) override {
    // Flush the buffer
    if (sync() < 0)
      return traits_type::eof();

    // Add the character if it's not EOF
    if (!traits_type::eq_int_type(ch, traits_type::eof())) {
      *pptr() = traits_type::to_char_type(ch);
      pbump(1);
    }

    return traits_type::not_eof(ch);
  }

  // Synchronize the buffer with the socket
  int sync() override {
    // Calculate bytes to write
    std::ptrdiff_t bytes_to_write = pptr() - pbase();
    if (bytes_to_write == 0)
      return 0;

    // Write to socket
    ssize_t bytes_written = 0;
    ssize_t result = 0;

    while (bytes_written < bytes_to_write) {
      result = write(socket_fd_, pbase() + bytes_written,
                     bytes_to_write - bytes_written);

      if (result < 0) {
        if (errno == EINTR) // Interrupted, try again
          continue;
        return -1; // Error
      }

      bytes_written += result;
    }

    // Reset put pointers
    setp(output_buffer_.data(), output_buffer_.data() + output_buffer_.size());

    return 0;
  }

private:
  int socket_fd_;
  std::vector<char> input_buffer_;
  std::vector<char> output_buffer_;
};

// Main tcp_stream class
class tcp_stream : public std::iostream {
public:
  // Constructors
  explicit tcp_stream(int socket_fd = -1)
      : std::iostream(nullptr), socket_fd_(socket_fd), streambuf_(socket_fd) {
    rdbuf(&streambuf_);
  }

  // No copy
  tcp_stream(const tcp_stream &) = delete;
  tcp_stream &operator=(const tcp_stream &) = delete;

  // Move
  tcp_stream(tcp_stream &&other) noexcept
      : std::iostream(std::move(other)),
        socket_fd_(std::exchange(other.socket_fd_, -1)),
        streambuf_(std::move(other.streambuf_)) {
    rdbuf(&streambuf_);
  }

  tcp_stream &operator=(tcp_stream &&other) noexcept {
    if (this != &other) {
      close();
      std::iostream::operator=(std::move(other));
      socket_fd_ = std::exchange(other.socket_fd_, -1);
      streambuf_ = std::move(other.streambuf_);
      rdbuf(&streambuf_);
    }
    return *this;
  }

  // Destructor
  ~tcp_stream() { close(); }

  // Check if the stream is connected
  bool is_open() const noexcept {
    return socket_fd_ >= 0 && streambuf_.is_open();
  }

  // Close the connection
  void close() {
    if (is_open()) {
      flush(); // Ensure all data is sent
      ::close(socket_fd_);
      socket_fd_ = -1;
    }
  }

  // Get the socket file descriptor
  int native_handle() const noexcept { return socket_fd_; }

  // Socket options

  // Set TCP_NODELAY (disable Nagle's algorithm)
  bool set_nodelay(bool enable) noexcept {
    int value = enable ? 1 : 0;
    return setsockopt(socket_fd_, IPPROTO_TCP, TCP_NODELAY, &value,
                      sizeof(value)) == 0;
  }

  // Set keep-alive
  bool set_keepalive(bool enable, int idle_time = 60, int interval = 10,
                     int count = 5) noexcept {
    int value = enable ? 1 : 0;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_KEEPALIVE, &value,
                   sizeof(value)) < 0)
      return false;

    if (enable) {
      // Set idle time
      if (setsockopt(socket_fd_, IPPROTO_TCP, TCP_KEEPIDLE, &idle_time,
                     sizeof(idle_time)) < 0)
        return false;

      // Set interval between probes
      if (setsockopt(socket_fd_, IPPROTO_TCP, TCP_KEEPINTVL, &interval,
                     sizeof(interval)) < 0)
        return false;

      // Set probe count
      if (setsockopt(socket_fd_, IPPROTO_TCP, TCP_KEEPCNT, &count,
                     sizeof(count)) < 0)
        return false;
    }

    return true;
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

  // Set receive timeout
  bool set_recv_timeout(int milliseconds) noexcept {
    struct timeval tv;
    tv.tv_sec = milliseconds / 1000;
    tv.tv_usec = (milliseconds % 1000) * 1000;
    return setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) ==
           0;
  }

  // Set send timeout
  bool set_send_timeout(int milliseconds) noexcept {
    struct timeval tv;
    tv.tv_sec = milliseconds / 1000;
    tv.tv_usec = (milliseconds % 1000) * 1000;
    return setsockopt(socket_fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) ==
           0;
  }

  // Check if socket is ready for reading (with timeout)
  bool is_readable(int timeout_ms = 0) const noexcept {
    struct pollfd pfd;
    pfd.fd = socket_fd_;
    pfd.events = POLLIN;
    pfd.revents = 0;

    return poll(&pfd, 1, timeout_ms) > 0 && (pfd.revents & POLLIN);
  }

  // Check if socket is ready for writing (with timeout)
  bool is_writable(int timeout_ms = 0) const noexcept {
    struct pollfd pfd;
    pfd.fd = socket_fd_;
    pfd.events = POLLOUT;
    pfd.revents = 0;

    return poll(&pfd, 1, timeout_ms) > 0 && (pfd.revents & POLLOUT);
  }

  // Connection
  static std::optional<tcp_stream> connect(std::string_view address,
                                           int timeout_ms = -1) {
    // Parse host:port format
    size_t colon_pos = address.find(':');
    if (colon_pos == std::string_view::npos) {
      return std::nullopt;
    }

    std::string host(address.substr(0, colon_pos));
    std::string port(address.substr(colon_pos + 1));

    // Set up address hints
    struct addrinfo hints = {}, *results = nullptr;
    hints.ai_family = AF_UNSPEC;     // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP

    // Get address info
    int gai_res = getaddrinfo(host.c_str(), port.c_str(), &hints, &results);
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

    // Try each address until we succeed
    for (struct addrinfo *addr = results; addr != nullptr;
         addr = addr->ai_next) {
      // Create socket
      int sock_fd =
          socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
      if (sock_fd < 0) {
        continue;
      }

      // Handle non-blocking connection with timeout
      if (timeout_ms >= 0) {
        // Set socket to non-blocking mode
        int flags = fcntl(sock_fd, F_GETFL, 0);
        fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);

        // Initiate connection
        int connect_res = ::connect(sock_fd, addr->ai_addr, addr->ai_addrlen);

        if (connect_res < 0 && errno == EINPROGRESS) {
          // Wait for connection to complete
          struct pollfd pfd;
          pfd.fd = sock_fd;
          pfd.events = POLLOUT;

          if (poll(&pfd, 1, timeout_ms) <= 0) {
            // Timeout or error
            ::close(sock_fd);
            continue;
          }

          // Check if connection succeeded
          int error = 0;
          socklen_t len = sizeof(error);
          if (getsockopt(sock_fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 ||
              error != 0) {
            ::close(sock_fd);
            continue;
          }

          // Reset socket to blocking mode
          fcntl(sock_fd, F_SETFL, flags);
        } else if (connect_res < 0) {
          // Connection failed immediately
          ::close(sock_fd);
          continue;
        }
      } else {
        // Blocking connection
        if (::connect(sock_fd, addr->ai_addr, addr->ai_addrlen) < 0) {
          ::close(sock_fd);
          continue;
        }
      }

      return tcp_stream(sock_fd);
    }

    return std::nullopt;
  }

private:
  int socket_fd_;
  tcp_streambuf streambuf_;
};

} // namespace net
