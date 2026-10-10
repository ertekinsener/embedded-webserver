#ifndef WEBSERVER_INCLUDE_WEBSERVER_UART_UART_SERVICE_
#define WEBSERVER_INCLUDE_WEBSERVER_UART_UART_SERVICE_

#include <cstdint>
#include <expected>
#include <span>

#include "webserver/uart/uart_types.h"

namespace webserver {

enum class UartError : std::uint8_t {
  kUnknownPort,
  kUnsupported,  // Out of range or not in the port's capabilities.
  kRejected,     // The port's driver refused the settings.
};

// The UART ports of the web UI: checks settings against each port's
// capabilities and hands them to its driver. Not thread safe: call from the
// HTTP server thread only.
class UartService {
 public:
  // Each port's `config` must hold the settings it currently runs with.
  explicit UartService(std::span<UartPort> ports) noexcept : ports_(ports) {}

  std::expected<void, UartError> Apply(const UartConfig& config) noexcept;

  std::span<const UartPort> ports() const noexcept { return ports_; }

 private:
  UartPort* Find(std::uint8_t id) noexcept;

  std::span<UartPort> ports_;
};

}  // namespace webserver

#endif  // WEBSERVER_INCLUDE_WEBSERVER_UART_UART_SERVICE_
