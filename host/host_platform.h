#ifndef WEBSERVER_HOST_HOST_PLATFORM_
#define WEBSERVER_HOST_HOST_PLATFORM_

#include <span>

#include "webserver/platform.h"
#include "webserver/system/system_api.h"
#include "webserver/uart/uart_types.h"

namespace host {

// Logger on stdout; call first.
void InitLogging() noexcept;

// Monotonic clock and getrandom().
webserver::Platform HostPlatform() noexcept;

webserver::SystemDescription HostSystem() noexcept;

// Simulated ports with different capabilities, so the UI can be tried with
// what different platforms offer (simulated_ports.cc).
std::span<webserver::UartPort> SimulatedPorts() noexcept;

}  // namespace host

#endif  // WEBSERVER_HOST_HOST_PLATFORM_
