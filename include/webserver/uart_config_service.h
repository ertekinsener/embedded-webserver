#ifndef APP_INCLUDE_WEBSERVER_UART_CONFIG_SERVICE_
#define APP_INCLUDE_WEBSERVER_UART_CONFIG_SERVICE_
#include "uart_config.h"

namespace webserver {
class UartConfigService {
 public:
  bool set_config(const UartConfig& config) noexcept;
  const UartConfig& config() const noexcept { return current_; }
 private:
  static bool validate(const UartConfig& config) noexcept;
  void apply_to_hardware(const UartConfig& config) noexcept;
  UartConfig current_{};
};

}  // namespace webserver
#endif /* APP_INCLUDE_WEBSERVER_UART_CONFIG_SERVICE_ */
