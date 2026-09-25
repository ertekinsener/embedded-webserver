#include "webserver/uart_config_service.h"

namespace webserver {
bool UartConfigService::set_config(const UartConfig& config) noexcept {
  if (!validate(config)) {
    return false;
  }
  current_ = config;
  // UART hardware configuration
  apply_to_hardware(config);
  return true;
}
bool UartConfigService::validate(const UartConfig& config) noexcept {
  return true;
}
void UartConfigService::apply_to_hardware(const UartConfig& config) noexcept {}
}  // namespace webserver