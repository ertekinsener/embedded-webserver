#ifndef WEBSERVER_INCLUDE_WEBSERVER_UART_UART_API_
#define WEBSERVER_INCLUDE_WEBSERVER_UART_UART_API_

#include "webserver/http_types.h"
#include "webserver/uart/uart_service.h"

namespace webserver {

// /api/uart/* handlers (schema/uart.fbs).
class UartApi {
 public:
  explicit UartApi(UartService& service) noexcept : service_(service) {}

  // GET /api/uart/ports: 200 UartPortList with every port's capabilities,
  // current settings and bridge settings and status.
  void ListPorts(const HttpRequest& request, HttpResponse& response) noexcept;

  // POST /api/uart/config: UartSettings -> 204; 404 unknown port, 422
  // unsupported or refused settings.
  void SetConfig(const HttpRequest& request, HttpResponse& response) noexcept;

  // POST /api/bridge/config: BridgeSettings -> 204; 404 unknown port, 422
  // no bridge on the port or invalid settings.
  void SetBridge(const HttpRequest& request, HttpResponse& response) noexcept;

 private:
  UartService& service_;
};

}  // namespace webserver

#endif  // WEBSERVER_INCLUDE_WEBSERVER_UART_UART_API_
