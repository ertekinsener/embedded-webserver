#ifndef WEBSERVER_ADAPTERS_POSIX_POSIX_HTTP_LISTENER_
#define WEBSERVER_ADAPTERS_POSIX_POSIX_HTTP_LISTENER_

#include <array>
#include <cstdint>

#include "webserver/http_server.h"
#include "webserver/http_types.h"

namespace webserver::posix {

// A minimal blocking HTTP/1.1 front end on BSD sockets, for running the web
// application on a PC (UI development, tests). One connection at a time,
// each closed after its response; no TLS, no chunked requests.
class HttpListener {
 public:
  explicit HttpListener(const HttpServer& server) noexcept
      : server_(server) {}

  HttpListener(const HttpListener&) = delete;
  HttpListener& operator=(const HttpListener&) = delete;

  // Listens on `port` (all interfaces) and serves until an accept() fails.
  // False if the socket could not be set up.
  bool Run(std::uint16_t port) noexcept;

  // Reads one request from the connected socket `client` and answers it.
  void Serve(int client) noexcept;

 private:
  bool SendResponse(int client) noexcept;

  const HttpServer& server_;
  std::array<char, kMaxRequestHeaderSize> header_buffer_{};
  alignas(8) std::array<std::uint8_t, kMaxRequestBodySize> body_buffer_{};
  HttpResponse response_;
};

}  // namespace webserver::posix

#endif  // WEBSERVER_ADAPTERS_POSIX_POSIX_HTTP_LISTENER_
