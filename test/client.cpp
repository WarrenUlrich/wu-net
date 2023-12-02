#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <chrono>
#include <array>

#include <wu-net/tcp_stream.hpp>
#include <wu-net/ipv4_address.hpp>

int main() {
  auto stream = net::tcp_stream::connect("127.0.0.1:8080");
  if (!stream.has_value()) {
    std::cout << "no stream.";
    return 1;
  }

  std::int32_t count = 0;
  while (true) {
    count++;
    *stream << count << '\n';
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    stream->flush();
  }
}

// int main() {
//   auto stream = net::tcp_stream::connect("example.com:80");
//   if (!stream) {
//     std::cerr << "Failed to connect to server" << std::endl;
//     return 1;
//   }

// 	stream->set_nonblocking(true);
	
//   *stream << "GET / HTTP/1.1\r\n"
//           << "Host: example.com\r\n"
//           << "Connection: close\r\n"
//           << "\r\n";
//   stream->flush(); // Ensure the request is sent immediately

//   while (!stream->is_ready_for_read(1)) {
//     std::cout << "Not ready :(\n";
//   }

// 	std::array<char, 1024> buffer;
// 	while (stream->getline(buffer.data(), buffer.size())) {
// 		std::cout << buffer.data() << '\n';
// 	}
//   return 0;
// }
