#include "webserver/uart/uart_service.h"

#include <array>
#include <cstdint>
#include <expected>
#include <utility>

#include "logging/logger.h"

namespace webserver {

std::expected<void, UartError> UartService::Apply(
    const UartConfig& config) noexcept {
  return ApplyConfig(config, true);
}

std::expected<void, UartError> UartService::ApplyTemporary(
    const UartConfig& config) noexcept {
  return ApplyConfig(config, false);
}

std::expected<void, UartError> UartService::ApplyConfig(
    const UartConfig& config, bool save) noexcept {
  const Guard guard(lock_);
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
  if (save) {
    port->saved_config = config;
  }
  logging::Info("uart", "{} set to {} baud", port->name, config.baud_rate);
  return {};
}

std::expected<void, UartError> UartService::ApplyBridge(
    std::uint8_t port_id, const UartBridgeSettings& settings) noexcept {
  const Guard guard(lock_);
  UartPort* port = Find(port_id);
  if (port == nullptr) {
    return std::unexpected(UartError::kUnknownPort);
  }
  const bool binds = settings.enabled &&
                     settings.mode != UartBridgeMode::kTcpClient;
  if (port->bridge_driver.apply == nullptr || !Valid(settings) ||
      (binds && LocalPortInUse(*port, settings.local_port))) {
    logging::Warning("uart", "{}: unsupported bridge settings", port->name);
    return std::unexpected(UartError::kUnsupported);
  }
  if (!port->bridge_driver.apply(port->bridge_driver.context, port->id,
                                 settings)) {
    return std::unexpected(UartError::kRejected);
  }
  port->bridge = settings;
  if (settings.enabled) {
    logging::Info("uart", "{} bridge on (mode {}, port {})", port->name,
                  settings.mode, settings.local_port);
  } else {
    logging::Info("uart", "{} bridge off", port->name);
  }
  return {};
}

bool UartService::Valid(const UartBridgeSettings& settings) noexcept {
  if (std::to_underlying(settings.mode) >
          std::to_underlying(UartBridgeMode::kUdp) ||
      settings.idle_gap_ms > kMaxBridgeIdleGapMs || settings.max_frame < 1 ||
      settings.max_frame > kMaxBridgeFrame) {
    return false;
  }
  if (!settings.enabled) {
    return true;
  }
  const bool has_remote_address =
      settings.remote_address != std::array<std::uint8_t, 4>{};
  switch (settings.mode) {
    case UartBridgeMode::kTcpServer:
    case UartBridgeMode::kRfc2217:
      return settings.local_port >= kMinBridgeLocalPort;
    case UartBridgeMode::kTcpClient:
      return has_remote_address && settings.remote_port != 0;
    case UartBridgeMode::kUdp:
      // The remote is optional (then: the last sender), but not half set.
      return settings.local_port >= kMinBridgeLocalPort &&
             has_remote_address == (settings.remote_port != 0);
  }
  return false;
}

bool UartService::GetConfig(std::uint8_t port_id,
                            UartConfig& config) const noexcept {
  const Guard guard(lock_);
  const UartPort* port = Find(port_id);
  if (port == nullptr) {
    return false;
  }
  config = port->config;
  return true;
}

bool UartService::LocalPortInUse(const UartPort& except,
                                 std::uint16_t local_port) const noexcept {
  for (const UartPort& port : ports_) {
    if (&port != &except && port.bridge.enabled &&
        port.bridge.mode != UartBridgeMode::kTcpClient &&
        port.bridge.local_port == local_port) {
      return true;
    }
  }
  return false;
}

UartPort* UartService::Find(std::uint8_t id) const noexcept {
  for (UartPort& port : ports_) {
    if (port.id == id) {
      return &port;
    }
  }
  return nullptr;
}

}  // namespace webserver
