#include "webserver/uart/uart_api.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include "uart_generated.h"
#include "webserver/flatbuffer_io.h"

namespace webserver {
namespace {

// Upper bounds of the encoded sizes (checked by the host tests), so the
// port list never outgrows the response buffer (see FlatBufferArena).
constexpr std::size_t kListBytes = 64;
constexpr std::size_t kPortBytes = 384;  // Plus the two strings.
constexpr std::size_t kMaxPorts = 16;

std::size_t EstimateListSize(std::span<const UartPort> ports) noexcept {
  std::size_t size = kListBytes;
  for (const UartPort& port : ports) {
    size += kPortBytes + port.name.size() + port.description.size();
  }
  return size;
}

// The enumerators of `set` as a FlatBuffers vector of the schema enum.
template <typename Schema, typename Enum>
flatbuffers::Offset<flatbuffers::Vector<Schema>> CreateEnumVector(
    flatbuffers::FlatBufferBuilder& builder, EnumSet<Enum> set) noexcept {
  std::array<Schema, 32> values{};
  std::size_t count = 0;
  for (std::uint8_t value = 0; value < values.size(); ++value) {
    if (set.Contains(static_cast<Enum>(value))) {
      values[count++] = static_cast<Schema>(value);
    }
  }
  return builder.CreateVector(values.data(), count);
}

flatbuffers::Offset<api::UartPort> CreatePort(
    flatbuffers::FlatBufferBuilder& builder, const UartPort& port) noexcept {
  const UartCapabilities& caps = port.capabilities;
  const auto name = builder.CreateString(port.name.data(), port.name.size());
  const auto description =
      builder.CreateString(port.description.data(), port.description.size());
  const auto word_lengths =
      CreateEnumVector<api::WordLength>(builder, caps.word_lengths);
  const auto stop_bits =
      CreateEnumVector<api::StopBits>(builder, caps.stop_bits);
  const auto parities = CreateEnumVector<api::Parity>(builder, caps.parities);
  const auto flow_controls =
      CreateEnumVector<api::FlowControl>(builder, caps.flow_controls);
  const auto oversampling =
      CreateEnumVector<api::Oversampling>(builder, caps.oversampling);
  const auto capabilities = api::CreateUartCapabilities(
      builder, caps.min_baud_rate, caps.max_baud_rate, word_lengths,
      stop_bits, parities, flow_controls, oversampling, caps.fifo,
      caps.tx_rx_swap);

  const UartConfig& config = port.config;
  const auto settings = api::CreateUartSettings(
      builder, port.id, config.baud_rate,
      static_cast<api::WordLength>(config.word_length),
      static_cast<api::StopBits>(config.stop_bits),
      static_cast<api::Parity>(config.parity),
      static_cast<api::FlowControl>(config.flow_control),
      static_cast<api::Oversampling>(config.oversampling),
      config.fifo_enabled, config.tx_rx_swap);
  if (port.bridge_driver.apply == nullptr) {
    return api::CreateUartPort(builder, port.id, name, description,
                               capabilities, settings);
  }
  const UartBridgeSettings& bridge = port.bridge;
  const auto remote_address = builder.CreateVector(
      bridge.remote_address.data(), bridge.remote_address.size());
  const auto bridge_settings = api::CreateBridgeSettings(
      builder, port.id, bridge.enabled,
      static_cast<api::BridgeMode>(bridge.mode), bridge.local_port,
      remote_address, bridge.remote_port, bridge.idle_gap_ms,
      bridge.max_frame, bridge.use_delimiter, bridge.delimiter);
  UartBridgeStatus status;
  if (port.bridge_driver.status != nullptr) {
    port.bridge_driver.status(port.bridge_driver.context, port.id, status);
  }
  const auto client_address = builder.CreateVector(
      status.client_address.data(),
      status.connected ? status.client_address.size() : 0);
  const auto bridge_status = api::CreateBridgeStatus(
      builder, status.listening, status.connected, client_address,
      status.client_port, status.uart_to_net_bytes, status.net_to_uart_bytes,
      status.dropped_bytes, status.uart_errors);
  return api::CreateUartPort(builder, port.id, name, description,
                             capabilities, settings, bridge_settings,
                             bridge_status);
}

void SendError(UartError error, HttpResponse& response) noexcept {
  switch (error) {
    case UartError::kUnknownPort:
      response.SetText(HttpStatus::kNotFound, "Unknown port");
      return;
    case UartError::kUnsupported:
      response.SetText(HttpStatus::kUnprocessableEntity,
                       "Settings not supported by this port");
      return;
    case UartError::kRejected:
      response.SetText(HttpStatus::kUnprocessableEntity,
                       "Settings rejected by the hardware");
      return;
  }
}

}  // namespace

void UartApi::ListPorts(const HttpRequest& /*request*/,
                        HttpResponse& response) noexcept {
  const UartService::Guard guard = service_.Locked();
  const std::span<const UartPort> ports = service_.ports();
  if (ports.size() > kMaxPorts ||
      EstimateListSize(ports) > response.buffer().size()) {
    response.SetText(HttpStatus::kInternalError, "Port list too large");
    return;
  }
  ResponseBuilder out(response);
  std::array<flatbuffers::Offset<api::UartPort>, kMaxPorts> offsets{};
  for (std::size_t i = 0; i < ports.size(); ++i) {
    offsets[i] = CreatePort(out.builder, ports[i]);
  }
  const auto list = out.builder.CreateVector(offsets.data(), ports.size());
  out.builder.Finish(api::CreateUartPortList(out.builder, list));
  out.Send(HttpStatus::kOk);
}

void UartApi::SetConfig(const HttpRequest& request,
                        HttpResponse& response) noexcept {
  const auto* settings = DecodeRequest<api::UartSettings>(request, response);
  if (settings == nullptr) {
    return;
  }
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
  const auto result = service_.Apply(config);
  if (!result) {
    SendError(result.error(), response);
    return;
  }
  response.SetEmpty(HttpStatus::kNoContent);
}

void UartApi::SetBridge(const HttpRequest& request,
                        HttpResponse& response) noexcept {
  const auto* bridge = DecodeRequest<api::BridgeSettings>(request, response);
  if (bridge == nullptr) {
    return;
  }
  UartBridgeSettings settings{
      .enabled = bridge->enabled(),
      // Range-checked by the service.
      .mode = static_cast<UartBridgeMode>(bridge->mode()),
      .local_port = bridge->local_port(),
      .remote_port = bridge->remote_port(),
      .idle_gap_ms = bridge->idle_gap_ms(),
      .max_frame = bridge->max_frame(),
      .use_delimiter = bridge->use_delimiter(),
      .delimiter = bridge->delimiter(),
  };
  if (const auto* address = bridge->remote_address()) {
    if (address->size() != settings.remote_address.size()) {
      response.SetText(HttpStatus::kBadRequest, "Bad remote address");
      return;
    }
    std::copy(address->begin(), address->end(),
              settings.remote_address.begin());
  }
  const auto result = service_.ApplyBridge(bridge->port_id(), settings);
  if (!result) {
    SendError(result.error(), response);
    return;
  }
  response.SetEmpty(HttpStatus::kNoContent);
}

}  // namespace webserver
