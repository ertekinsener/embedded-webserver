#ifndef WEBSERVER_INCLUDE_WEBSERVER_UART_UART_CODEC_
#define WEBSERVER_INCLUDE_WEBSERVER_UART_UART_CODEC_

#include <cstdint>
#include <optional>

#include "flatbuffers/flatbuffer_builder.h"
#include "uart_generated.h"
#include "webserver/uart/uart_types.h"

// The UART types to and from their FlatBuffers tables (schema/uart.fbs), for
// the API and the saved settings. Decoded enums are cast, not checked:
// UartService range-checks them.
namespace webserver {

flatbuffers::Offset<api::UartSettings> CreateUartSettings(
    flatbuffers::FlatBufferBuilder& builder, std::uint8_t port_id,
    const UartConfig& config) noexcept;

UartConfig ToUartConfig(const api::UartSettings& settings) noexcept;

flatbuffers::Offset<api::BridgeSettings> CreateBridgeSettings(
    flatbuffers::FlatBufferBuilder& builder, std::uint8_t port_id,
    const UartBridgeSettings& settings) noexcept;

// nullopt if the remote address is not 4 bytes.
std::optional<UartBridgeSettings> ToBridgeSettings(
    const api::BridgeSettings& settings) noexcept;

}  // namespace webserver

#endif  // WEBSERVER_INCLUDE_WEBSERVER_UART_UART_CODEC_
