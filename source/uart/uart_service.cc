#include "webserver/uart/uart_service.h"

#include <cstdint>
#include <expected>

#include "logging/logger.h"

namespace webserver {

std::expected<void, UartError> UartService::Apply(
    const UartConfig& config) noexcept {
  UartPort* port = Find(config.port_id);
  if (port == nullptr) {
    return std::unexpected(UartError::kUnknownPort);
  }
  // The enums come from an untrusted FlatBuffer by casting; Supports() also
  // rejects values outside the enums.
  if (!Supports(port->capabilities, config)) {
    logging::Warning("uart", "{}: unsupported settings", port->name);
    return std::unexpected(UartError::kUnsupported);
  }
  if (port->driver.apply == nullptr ||
      !port->driver.apply(port->driver.context, config)) {
    logging::Warning("uart", "{}: settings rejected by the driver",
                     port->name);
    return std::unexpected(UartError::kRejected);
  }
  port->config = config;
  logging::Info("uart", "{} set to {} baud", port->name, config.baud_rate);
  return {};
}

UartPort* UartService::Find(std::uint8_t id) noexcept {
  for (UartPort& port : ports_) {
    if (port.id == id) {
      return &port;
    }
  }
  return nullptr;
}

}  // namespace webserver
