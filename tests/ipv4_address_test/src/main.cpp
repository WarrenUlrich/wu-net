#include <iostream>

#include <wu-net/ipv4_address.hpp>

int main(int argc, char **args) {
  auto test_addr = net::ipv4_address(73, 196, 6, 173);
  if (test_addr.to_string() != "73.196.6.173") {
    return 1;
  }

  test_addr =
      net::ipv4_address::from_str("127.0.0.1").value_or(net::ipv4_address());
  if (test_addr.to_string() != "127.0.0.1") {
    return 1;
  }

  auto resolved = net::ipv4_address::resolve_host("www.youtube.com");
  if (!resolved) {
    std::cout << "Failed to resolve host\n";
    return 1;
  }

  std::cout << resolved->to_string() << '\n';
  std::cout << "passed\n";
  return 0;
}