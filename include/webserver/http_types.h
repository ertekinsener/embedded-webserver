#ifndef APP_INCLUDE_WEBSERVER_HTTP_TYPES_
#define APP_INCLUDE_WEBSERVER_HTTP_TYPES_

#include <array>
#include <cstddef>
#include <cstring>
#include <string_view>

namespace webserver {
enum class HttpMethod {
  Get,
  Head,
  Post,
  Put,
  Delete,
  Connect,
  Options,
  Trace,
  Unknown
};

struct HttpRequest {
  HttpMethod method;
  std::string_view path;
  std::string_view content_type;
  std::string_view body;
};

struct HttpResponse {
  static constexpr std::size_t kOwnedCapacity = 512;
  int status{500};
  std::string_view content_type{"text/plain"};
  const char* body{nullptr};
  std::size_t body_size{0};
  std::array<char, kOwnedCapacity> owned_body{};

  void set_body(const char* data, std::size_t size,
                std::string_view type) noexcept {
    body_size = size;
    content_type = type;
    if (data != nullptr && size <= owned_body.size()) {
      std::memcpy(owned_body.data(), data, size);
      body = owned_body.data();
    } else {
      body = data;
    }
  }

  void set_owned_body(std::size_t size, std::string_view type) noexcept {
    body_size = size;
    content_type = type;
    body = owned_body.data();
  }

  HttpResponse() = default;
  HttpResponse(const HttpResponse& other) noexcept
      : status(other.status),
        content_type(other.content_type),
        body_size(other.body_size),
        owned_body(other.owned_body) {
    body =
        other.body == other.owned_body.data() ? owned_body.data() : other.body;
  }
  HttpResponse& operator=(const HttpResponse& other) noexcept {
    if (this != &other) {
      status = other.status;
      content_type = other.content_type;
      body_size = other.body_size;
      owned_body = other.owned_body;
      body = other.body == other.owned_body.data() ? owned_body.data()
                                                   : other.body;
    }
    return *this;
  }
};

}  // namespace webserver


#endif /* APP_INCLUDE_WEBSERVER_HTTP_TYPES_ */
