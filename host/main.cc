// The web application on a PC with simulated UART ports:
//
//   webserver_host [port [settings-file]]
//
// Port 8080 by default, login from WEBSERVER_ADMIN_*. Settings changed in
// the UI are saved to `settings-file` and restored from it at start;
// without one they are kept in memory only.

#include <array>
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
      .settings_storage =
          host::HostSettingsStorage(argc > 2 ? argv[2] : nullptr),
  }};
  static std::array<std::uint8_t, webserver::SettingsStore::kMaxSize> scratch;
  web.settings().Restore(scratch);
  static webserver::posix::HttpListener listener{web.server()};
  if (!listener.Run(port)) {
    logging::Error("host", "cannot listen on port {}", port);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
