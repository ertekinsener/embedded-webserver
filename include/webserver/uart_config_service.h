#ifndef APP_INCLUDE_WEBSERVER_UART_CONFIG_SERVICE_
#define APP_INCLUDE_WEBSERVER_UART_CONFIG_SERVICE_

#include <cstdint>
#include <span>

#include "webserver/uart_config.h"

namespace webserver {

// Connects a web UI port id to the code that applies settings to that port's
// hardware. `apply` returns false if the hardware rejects the settings.
struct UartPortBinding {
  using Apply = bool (*)(void* context, const UartConfig& config) noexcept;

  std::uint8_t port_id;
  Apply apply;
  void* context;
};

// Range-checks decoded settings and hands them to the bound port; whether
// the hardware supports a combination is decided by the port.
class UartConfigService {
 public:
  explicit UartConfigService(std::span<const UartPortBinding> ports) noexcept
      : ports_(ports) {}

  // False if the settings are out of range, the port is unknown, or the
  // port rejected them. The last accepted settings are kept in config().
  bool SetConfig(const UartConfig& config) noexcept;
  const UartConfig& config() const noexcept { return current_; }

 private:
  static bool IsInRange(const UartConfig& config) noexcept;
  const UartPortBinding* FindPort(std::uint8_t port_id) const noexcept;

  std::span<const UartPortBinding> ports_;
  UartConfig current_{};
};

}  // namespace webserver

#endif  // APP_INCLUDE_WEBSERVER_UART_CONFIG_SERVICE_
