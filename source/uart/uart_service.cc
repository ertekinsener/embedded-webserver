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

std::expected<void, UartError> UartService::ApplyBridge(
    std::uint8_t port_id, const UartBridgeSettings& settings) noexcept {
  UartPort* port = Find(port_id);
  if (port == nullptr) {
    return std::unexpected(UartError::kUnknownPort);
  }
  const bool valid =
      port->bridge_driver.apply != nullptr &&
      settings.idle_gap_ms <= kMaxBridgeIdleGapMs &&
      settings.max_frame >= 1 && settings.max_frame <= kMaxBridgeFrame &&
      (!settings.enabled || (settings.tcp_port >= kMinBridgeTcpPort &&
                             !TcpPortInUse(*port, settings.tcp_port)));
  if (!valid) {
    logging::Warning("uart", "{}: unsupported bridge settings", port->name);
    return std::unexpected(UartError::kUnsupported);
  }
  if (!port->bridge_driver.apply(port->bridge_driver.context, port->id,
                                 settings)) {
    return std::unexpected(UartError::kRejected);
  }
  port->bridge = settings;
  if (settings.enabled) {
    logging::Info("uart", "{} bridged to TCP port {}", port->name,
                  settings.tcp_port);
  } else {
    logging::Info("uart", "{} bridge off", port->name);
  }
  return {};
}

bool UartService::TcpPortInUse(const UartPort& except,
                               std::uint16_t tcp_port) const noexcept {
  for (const UartPort& port : ports_) {
    if (&port != &except && port.bridge.enabled &&
        port.bridge.tcp_port == tcp_port) {
      return true;
    }
  }
  return false;
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
