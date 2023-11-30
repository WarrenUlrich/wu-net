#include <iostream>

#include <wu-net/tcp_stream.hpp>
#include <wu-net/tcp_listener.hpp>

int main(int argc, char **args) {
  auto listener = net::tcp_listener::bind("127.0.0.1:8080");
  if (!listener.has_value()) {
    std::cout << "Failed to bind listener\n";
  }
}