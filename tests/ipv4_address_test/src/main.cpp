// #include <iostream>
// #include <generator>

// #include <wu-net/ipv4_address.hpp>

// int main(int argc, char **args) {
//   auto test_addr = net::ipv4_address(73, 196, 6, 173);
//   if (test_addr.to_string() != "73.196.6.173") {
//     return 1;
//   }

//   test_addr =
//       net::ipv4_address::from_str("127.0.0.1").value_or(net::ipv4_address());
//   if (test_addr.to_string() != "127.0.0.1") {
//     return 1;
//   }

//   auto resolved = net::ipv4_address::resolve_host("www.youtube.com");
//   if (!resolved) {
//     std::cout << "Failed to resolve host\n";
//     return 1;
//   }

//   std::cout << resolved->to_string() << '\n';
//   std::cout << "passed\n";
//   return 0;
// }

#include <wu-net/net.hpp>

#include <iostream>
#include <thread>

int main() {
    // Create a listener on port 8080
    auto listener = net::tcp_listener::create("*:8080");
    if (!listener) {
        std::cerr << "Failed to create listener\n";
        return 1;
    }
    
    std::cout << "Listening on " << listener->local_address().value_or("unknown") << std::endl;
    
    // Flag to stop accepting connections
    bool should_stop = false;
    
    // In another thread or signal handler, you could set should_stop = true
    
    // Iterate over incoming connections using the generator
    for (auto client : listener->connections(&should_stop)) {
        std::cout << "Client connected\n";
        
        // Use the tcp_stream object
        std::string line;
        while (std::getline(client, line)) {
            std::cout << "Received: " << line << std::endl;
            
            // Echo back
            client << "Echo: " << line << std::endl;
            
            if (line == "quit") {
                break;
            }
        }
        
        std::cout << "Client disconnected\n";
    }
    
    return 0;
}