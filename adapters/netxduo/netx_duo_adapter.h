#ifndef WEBSERVER_ADAPTERS_NETXDUO_NETX_DUO_ADAPTER_
#define WEBSERVER_ADAPTERS_NETXDUO_NETX_DUO_ADAPTER_

#include <array>
#include <cstdint>

#include "nx_api.h"
#include "nx_web_http_server.h"
#include "webserver/http_server.h"
#include "webserver/http_types.h"

namespace webserver::netx_duo {

// Connects HttpServer to the NetX Duo web server addon. Call HandleRequest()
// from the request notify callback passed to nx_web_http_server_create():
// it copies the request headers and body into the adapter's buffers, lets
// the HttpServer handle it and sends the response (large bodies in several
// packets). The NetX web server runs one thread, so one adapter serves all
// connections.
class Adapter {
 public:
  explicit Adapter(const HttpServer& server) noexcept : server_(server) {}

  Adapter(const Adapter&) = delete;
  Adapter& operator=(const Adapter&) = delete;

  // `request_type` is the callback's NX_WEB_HTTP_SERVER_*_REQUEST value.
  // Returns NX_WEB_HTTP_CALLBACK_COMPLETED unless sending failed.
  UINT HandleRequest(NX_WEB_HTTP_SERVER* server, UINT request_type,
                     CHAR* resource, NX_PACKET* packet) noexcept;

 private:
  UINT SendResponse(NX_WEB_HTTP_SERVER* server) noexcept;

  const HttpServer& server_;
  std::array<char, kMaxRequestHeaderSize> header_buffer_{};
  alignas(8) std::array<std::uint8_t, kMaxRequestBodySize> body_buffer_{};
  HttpResponse response_;
};

}  // namespace webserver::netx_duo

#endif  // WEBSERVER_ADAPTERS_NETXDUO_NETX_DUO_ADAPTER_
