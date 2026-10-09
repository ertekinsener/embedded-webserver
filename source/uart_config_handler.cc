#include "webserver/uart_config_handler.h"

#include <cstdint>

#include "flatbuffers/verifier.h"
#include "uart_config_generated.h"

namespace webserver {

void UartConfigHandler::HandlePost(const HttpRequest& request,
                                   HttpResponse& response) noexcept {
  if (request.content_type != "application/octet-stream") {
    response.SetText(HttpStatus::kUnsupportedMediaType,
                     "Unsupported Media Type");
    return;
  }
  // The body comes from the network: verify every offset before reading.
  const auto* data = reinterpret_cast<const std::uint8_t*>(request.body.data());
  flatbuffers::Verifier verifier(data, request.body.size());
  if (request.body.empty() ||
      !UartBridge::Config::VerifyUartSettingsBuffer(verifier)) {
    response.SetText(HttpStatus::kBadRequest, "Invalid FlatBuffer");
    return;
  }
  const auto* settings = UartBridge::Config::GetUartSettings(data);

  // Range-checked by the service.
  const UartConfig config{
      .port_id = settings->port_id(),
      .baud_rate = settings->baud_rate(),
      .word_length = static_cast<UartWordLength>(settings->word_length()),
      .stop_bits = static_cast<UartStopBits>(settings->stop_bits()),
      .parity = static_cast<UartParity>(settings->parity()),
      .flow_control = static_cast<UartFlowControl>(settings->flow_control()),
      .oversampling = static_cast<UartOversampling>(settings->oversampling()),
      .fifo_enabled = settings->fifo_enabled(),
      .tx_rx_swap = settings->tx_rx_swap(),
  };
  if (!service_.SetConfig(config)) {
    response.SetText(HttpStatus::kUnprocessableEntity,
                     "Invalid UART configuration");
    return;
  }
  response.status = HttpStatus::kNoContent;
  response.SetEmptyBody("application/octet-stream");
}

}  // namespace webserver
