#include "webserver/uart_config_handler.h"
#include "uart_config_generated.h"
namespace webserver {
void UartConfigHandler::handle_post(const HttpRequest& request,
                                    HttpResponse& response) noexcept {
  if (request.content_type != "application/octet-stream") {
    response.status = 415;
    response.set_body("Unsupported Media Type", 22, "text/plain");
    return;
  }

  if (request.body.empty()) {
    response.status = 400;
    response.set_body("Empty body", 10, "text/plain");
    return;
  }

  const auto* settings =
      UartBridge::Config::GetUartSettings(request.body.data());

  if (settings == nullptr) {
    response.status = 400;
    response.set_body("Invalid FlatBuffer", 18, "text/plain");
    return;
  }

  UartConfig config{
      .port_id = settings->port_id(),
      .baud_rate = settings->baud_rate(),
      .word_length = static_cast<UartWordLength>(settings->word_length()), // enum order should match between UartConfig and UartSettings
      .stop_bits = static_cast<UartStopBits>(settings->stop_bits()),
      .parity = static_cast<UartParity>(settings->parity()),
      .flow_control = static_cast<UartFlowControl>(settings->flow_control()),
      .oversampling = static_cast<UartOversampling>(settings->oversampling()),
      .fifo_enabled = settings->fifo_enabled(),
      .tx_rx_swap = settings->tx_rx_swap(),
  };

  if (!service_.set_config(config)) {
    response.status = 422;
    response.set_body("Invalid UART configuration", 26, "text/plain");
    return;
  }

  response.status = 204;
  response.set_owned_body(0, "application/octet-stream");
}
}  // namespace webserver