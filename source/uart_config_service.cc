#include "webserver/uart_config_service.h"

#include <utility>

namespace webserver {
bool UartConfigService::set_config(const UartConfig& config) noexcept {
  if (!validate(config)) {
    return false;
  }
  const UartPortBinding* port = find(config.port_id);
  if (port == nullptr || !port->apply(port->context, config)) {
    return false;
  }
  current_ = config;
  return true;
}

// Checks that the enums decoded from the FlatBuffer are in range. Whether the
// hardware supports a combination is decided by the bound port.
bool UartConfigService::validate(const UartConfig& config) noexcept {
  return config.baud_rate != 0 &&
         std::to_underlying(config.word_length) <=
             std::to_underlying(UartWordLength::Bits9) &&
         std::to_underlying(config.stop_bits) <=
             std::to_underlying(UartStopBits::Bits1_5) &&
         std::to_underlying(config.parity) <=
             std::to_underlying(UartParity::Odd) &&
         std::to_underlying(config.flow_control) <=
             std::to_underlying(UartFlowControl::RTS_CTS) &&
         std::to_underlying(config.oversampling) <=
             std::to_underlying(UartOversampling::Over8);
}

const UartPortBinding* UartConfigService::find(
    std::uint8_t port_id) const noexcept {
  for (const UartPortBinding& port : ports_) {
    if (port.port_id == port_id) {
      return &port;
    }
  }
  return nullptr;
}
}  // namespace webserver
