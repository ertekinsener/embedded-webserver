#ifndef APP_INCLUDE_WEBSERVER_UART_CONFIG_HANDLER_
#define APP_INCLUDE_WEBSERVER_UART_CONFIG_HANDLER_

#include "webserver/http_types.h"
#include "webserver/uart_config_service.h"

namespace webserver {

// POST /api/uart/config: decodes and verifies the FlatBuffer
// (web/schema/uart_config.fbs) and applies it through the service.
// 204 on success; 400 for a malformed body, 415 for a wrong content type,
// 422 if the settings are rejected.
class UartConfigHandler {
 public:
  explicit UartConfigHandler(UartConfigService& service) noexcept
      : service_(service) {}

  void HandlePost(const HttpRequest& request, HttpResponse& response) noexcept;

 private:
  UartConfigService& service_;
};

}  // namespace webserver

#endif  // APP_INCLUDE_WEBSERVER_UART_CONFIG_HANDLER_
