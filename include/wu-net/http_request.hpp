#pragma once

#include <string>
#include <unordered_map>
#include <chrono>

namespace net {
enum class http_method {
  GET,
  POST,
  PUT,
  DELETE,
  HEAD,
  OPTIONS,
  PATCH
};

enum class http_version {
  HTTP_1_0,
  HTTP_1_1,
  HTTP_2_0
};

class http_request {
public:
  
private:
  http_method _method;
  std::string _url;
  http_version _version;
  std::string _host;
  int _port;
  std::unordered_map<std::string, std::string> _headers;
  std::string _body;
  bool _keep_alive;
  std::chrono::milliseconds _timeout;
};
} // namespace net