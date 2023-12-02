#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <chrono>
#include <array>

#include <wu-net/tcp_listener.hpp>

int main() {
  auto listener = net::tcp_listener::bind("127.0.0.1:8080");
  if (!listener.has_value()) {
    std::cout << "no listener.";
    return 1;
  }

  while(true) {
    auto conn = listener->accept();
    if (!conn) {
      std::cout << "Failed to accept connection.";
      return 1;
    }

    std::cout << "connection input:\n";
    std::string line;
    while(std::getline(*conn, line)) {
      std::cout << line << '\n';
    }
  }
}