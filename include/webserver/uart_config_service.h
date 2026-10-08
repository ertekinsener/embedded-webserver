#ifndef APP_INCLUDE_WEBSERVER_UART_CONFIG_SERVICE_
#define APP_INCLUDE_WEBSERVER_UART_CONFIG_SERVICE_

#include <cstdint>
#include <span>

#include "uart_config.h"

namespace webserver {

// Connects a web UI port id to the code that applies settings to that port's
// hardware. `apply` returns false if the hardware rejects the settings.
struct UartPortBinding {
  using Apply = bool (*)(void* context, const UartConfig& config) noexcept;

  std::uint8_t port_id;
  Apply apply;
  void* context;
};

class UartConfigService {
 public:
  explicit UartConfigService(std::span<const UartPortBinding> ports) noexcept
      : ports_(ports) {}

  bool set_config(const UartConfig& config) noexcept;
  const UartConfig& config() const noexcept { return current_; }

 private:
  static bool validate(const UartConfig& config) noexcept;
  const UartPortBinding* find(std::uint8_t port_id) const noexcept;

  std::span<const UartPortBinding> ports_;
  UartConfig current_{};
};

}  // namespace webserver
#endif /* APP_INCLUDE_WEBSERVER_UART_CONFIG_SERVICE_ */
