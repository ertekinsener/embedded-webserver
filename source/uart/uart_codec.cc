#include "webserver/uart/uart_codec.h"

#include <algorithm>
#include <cstdint>
#include <optional>

namespace webserver {

flatbuffers::Offset<api::UartSettings> CreateUartSettings(
    flatbuffers::FlatBufferBuilder& builder, std::uint8_t port_id,
    const UartConfig& config) noexcept {
  return api::CreateUartSettings(
      builder, port_id, config.baud_rate,
      static_cast<api::WordLength>(config.word_length),
      static_cast<api::StopBits>(config.stop_bits),
      static_cast<api::Parity>(config.parity),
      static_cast<api::FlowControl>(config.flow_control),
      static_cast<api::Oversampling>(config.oversampling),
      config.fifo_enabled, config.tx_rx_swap);
}

UartConfig ToUartConfig(const api::UartSettings& settings) noexcept {
  return {
      .port_id = settings.port_id(),
      .baud_rate = settings.baud_rate(),
      .word_length = static_cast<UartWordLength>(settings.word_length()),
      .stop_bits = static_cast<UartStopBits>(settings.stop_bits()),
      .parity = static_cast<UartParity>(settings.parity()),
      .flow_control = static_cast<UartFlowControl>(settings.flow_control()),
      .oversampling = static_cast<UartOversampling>(settings.oversampling()),
      .fifo_enabled = settings.fifo_enabled(),
      .tx_rx_swap = settings.tx_rx_swap(),
  };
}

flatbuffers::Offset<api::BridgeSettings> CreateBridgeSettings(
    flatbuffers::FlatBufferBuilder& builder, std::uint8_t port_id,
    const UartBridgeSettings& settings) noexcept {
  const auto remote_address = builder.CreateVector(
      settings.remote_address.data(), settings.remote_address.size());
  return api::CreateBridgeSettings(
      builder, port_id, settings.enabled,
      static_cast<api::BridgeMode>(settings.mode), settings.local_port,
      remote_address, settings.remote_port, settings.idle_gap_ms,
      settings.max_frame, settings.use_delimiter, settings.delimiter);
}

std::optional<UartBridgeSettings> ToBridgeSettings(
    const api::BridgeSettings& settings) noexcept {
  UartBridgeSettings result{
      .enabled = settings.enabled(),
      .mode = static_cast<UartBridgeMode>(settings.mode()),
      .local_port = settings.local_port(),
      .remote_port = settings.remote_port(),
      .idle_gap_ms = settings.idle_gap_ms(),
      .max_frame = settings.max_frame(),
      .use_delimiter = settings.use_delimiter(),
      .delimiter = settings.delimiter(),
  };
  if (const auto* address = settings.remote_address()) {
    if (address->size() != result.remote_address.size()) {
      return std::nullopt;
    }
    std::copy(address->begin(), address->end(),
              result.remote_address.begin());
  }
  return result;
}

}  // namespace webserver
