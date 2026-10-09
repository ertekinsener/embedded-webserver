#include "webserver/uart_config_service.h"

#include <utility>

namespace webserver {

bool UartConfigService::SetConfig(const UartConfig& config) noexcept {
  if (!IsInRange(config)) {
    return false;
  }
  const UartPortBinding* port = FindPort(config.port_id);
  if (port == nullptr || !port->apply(port->context, config)) {
    return false;
  }
  current_ = config;
  return true;
}

// The enums come from an untrusted FlatBuffer by casting.
bool UartConfigService::IsInRange(const UartConfig& config) noexcept {
  return config.baud_rate != 0 &&
         std::to_underlying(config.word_length) <=
             std::to_underlying(UartWordLength::kBits9) &&
         std::to_underlying(config.stop_bits) <=
             std::to_underlying(UartStopBits::kOneAndHalf) &&
         std::to_underlying(config.parity) <=
             std::to_underlying(UartParity::kOdd) &&
         std::to_underlying(config.flow_control) <=
             std::to_underlying(UartFlowControl::kRtsCts) &&
         std::to_underlying(config.oversampling) <=
             std::to_underlying(UartOversampling::kBy8);
}

const UartPortBinding* UartConfigService::FindPort(
    std::uint8_t port_id) const noexcept {
  for (const UartPortBinding& port : ports_) {
    if (port.port_id == port_id) {
      return &port;
    }
  }
  return nullptr;
}

}  // namespace webserver
