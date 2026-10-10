// The web application on a PC with simulated UART ports:
//
//   webserver_host [port]      (default 8080, login from WEBSERVER_ADMIN_*)

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <string_view>

#include "host_platform.h"
#include "logging/logger.h"
#include "posix_http_listener.h"
#include "web_resources.h"
#include "webserver/auth/build_credentials.h"
#include "webserver/web_application.h"

int main(int argc, char** argv) {
  host::InitLogging();
  std::uint16_t port = 8080;
  if (argc > 1) {
    const std::string_view text(argv[1]);
    std::from_chars(text.data(), text.data() + text.size(), port);
  }

  static webserver::WebApplication web{{
      .resources = webserver::WebResources(),
      .uart_ports = host::SimulatedPorts(),
      .credentials = webserver::auth::BuildCredentials(),
      .platform = host::HostPlatform(),
      .system = host::HostSystem(),
  }};
  static webserver::posix::HttpListener listener{web.server()};
  if (!listener.Run(port)) {
    logging::Error("host", "cannot listen on port {}", port);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
