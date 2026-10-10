#ifndef WEBSERVER_INCLUDE_WEBSERVER_WEB_APPLICATION_
#define WEBSERVER_INCLUDE_WEBSERVER_WEB_APPLICATION_

#include <array>
#include <span>

#include "webserver/api_router.h"
#include "webserver/auth/auth_api.h"
#include "webserver/auth/session_manager.h"
#include "webserver/http_server.h"
#include "webserver/platform.h"
#include "webserver/resource_registry.h"
#include "webserver/system/system_api.h"
#include "webserver/uart/uart_api.h"
#include "webserver/uart/uart_service.h"

// Every API path, for HTTP servers that register paths up front (Zephyr):
// expands X(<args>..., n, path) per path. Must list every route of
// WebApplication (checked by the host tests).
#define WEBSERVER_API_PATHS(X, ...)      \
  X(__VA_ARGS__, 0, "/api/auth/login")   \
  X(__VA_ARGS__, 1, "/api/auth/logout")  \
  X(__VA_ARGS__, 2, "/api/system/info")  \
  X(__VA_ARGS__, 3, "/api/uart/ports")   \
  X(__VA_ARGS__, 4, "/api/uart/config")  \
  X(__VA_ARGS__, 5, "/api/bridge/config")

namespace webserver {

struct WebApplicationConfig {
  std::span<const WebResource> resources;  // WebResources() of the UI build
  std::span<UartPort> uart_ports;
  auth::Credentials credentials{};  // auth::BuildCredentials()
  Platform platform{};
  SystemDescription system{};
  auth::SessionPolicy session_policy{};
};

// The whole web application, independent of the network stack: embedded UI,
// login and the UART and system APIs. A platform creates one and connects
// server() to its network stack through an adapter (adapters/*):
//
//   webserver::WebApplication web{{.resources = webserver::WebResources(),
//                                  .uart_ports = ports, ...}};
//   webserver::netx_duo::Adapter adapter{web.server()};
class WebApplication {
 public:
  explicit WebApplication(const WebApplicationConfig& config) noexcept;

  WebApplication(const WebApplication&) = delete;
  WebApplication& operator=(const WebApplication&) = delete;

  const HttpServer& server() const noexcept { return server_; }
  std::span<const ApiRouter::Route> routes() const noexcept { return routes_; }

 private:
  ResourceRegistry resources_;
  auth::SessionManager sessions_;
  UartService uart_service_;
  auth::AuthApi auth_api_;
  UartApi uart_api_;
  SystemApi system_api_;
  std::array<ApiRouter::Route, 6> routes_;
  ApiRouter router_;
  HttpServer server_;
};

}  // namespace webserver

#endif  // WEBSERVER_INCLUDE_WEBSERVER_WEB_APPLICATION_
