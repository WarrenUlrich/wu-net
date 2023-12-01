#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <chrono>
#include <array>

#include <wu-net/tcp_stream.hpp>

int main() {
  auto stream = net::tcp_stream::connect("example.com:80");
  if (!stream) {
    std::cerr << "Failed to connect to server" << std::endl;
    return 1;
  }

	stream->set_nonblocking(false);
	
  *stream << "GET / HTTP/1.1\r\n"
          << "Host: example.com\r\n"
          << "Connection: close\r\n"
          << "\r\n";
  stream->flush(); // Ensure the request is sent immediately

	std::array<char, 1024> buffer;
	while (stream->getline(buffer.data(), buffer.size())) {
		std::cout << buffer.data() << '\n';
	}
  return 0;
}