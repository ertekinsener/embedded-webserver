#ifndef APP_INCLUDE_WEBSERVER_NETX_DUO_ADAPTER_
#define APP_INCLUDE_WEBSERVER_NETX_DUO_ADAPTER_

#include <cstddef>

#include "nx_api.h"
#include "nx_web_http_server.h"
#include "webserver/http_server.h"

namespace webserver::netx_duo {

// NetX Duo web server request callback body: copies the request headers
// and body into buffers on the HTTP thread's stack, lets `http_server`
// handle it and sends the response (400 if the request is larger than the
// buffers). Returns NX_WEB_HTTP_CALLBACK_COMPLETED unless sending failed.
inline constexpr std::size_t kMaxHeaderSize = 2048;
inline constexpr std::size_t kMaxBodySize = 512;

UINT HandleRequest(const HttpServer& http_server, NX_WEB_HTTP_SERVER* server,
                   UINT request_type, CHAR* resource,
                   NX_PACKET* packet) noexcept;

}  // namespace webserver::netx_duo

#endif  // APP_INCLUDE_WEBSERVER_NETX_DUO_ADAPTER_
