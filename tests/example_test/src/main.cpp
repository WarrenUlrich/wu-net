#include <iostream>
#include <string>

#include <wu-net/net.hpp>

int main() {
  // Connect to www.example.com on port 80 (HTTP)
  auto connection = net::tcp_stream::connect("www.example.com:80");

  if (!connection) {
    std::cerr << "Failed to connect to www.example.com" << std::endl;
    return 1;
  }

  std::cout << "Connected to www.example.com" << std::endl;

  // Create an HTTP GET request
  std::string request = "GET / HTTP/1.1\r\n"
                        "Host: www.example.com\r\n"
                        "Connection: close\r\n"
                        "User-Agent: tcp_stream_client/1.0\r\n"
                        "\r\n";

  // Send the request
  *connection << request;
  connection->flush(); // Ensure all data is sent immediately

  std::cout << "Request sent, awaiting response..." << std::endl;

  // Read and print the response
  std::string line;
  std::cout << "\n----- RESPONSE -----\n" << std::endl;

  while (std::getline(*connection, line)) {
    std::cout << line << std::endl;
  }

  std::cout << "\n----- END OF RESPONSE -----" << std::endl;

  // The connection will be closed automatically when it goes out of scope,
  // but we can explicitly close it too
  connection->close();

  return 0;
}
