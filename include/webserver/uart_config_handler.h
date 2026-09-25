#ifndef APP_INCLUDE_WEBSERVER_UART_CONFIG_HANDLER_
#define APP_INCLUDE_WEBSERVER_UART_CONFIG_HANDLER_
#include "webserver/uart_config_service.h"
#include "webserver/http_types.h"

namespace webserver {
class UartConfigHandler {
 public:
  explicit UartConfigHandler(UartConfigService& service) noexcept
      : service_(service) {}

  void handle_get(const HttpRequest& request, HttpResponse& response) noexcept;

  void handle_post(const HttpRequest& request, HttpResponse& response) noexcept;

 private:
  UartConfigService& service_;
};
}  // namespace webserver
#endif /* APP_INCLUDE_WEBSERVER_UART_CONFIG_HANDLER_ */
