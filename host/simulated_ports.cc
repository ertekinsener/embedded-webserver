#include <array>
#include <span>

#include "host_platform.h"
#include "logging/logger.h"
#include "webserver/uart/uart_types.h"

namespace host {
namespace {

using webserver::UartCapabilities;
using webserver::UartConfig;
using webserver::UartFlowControl;
using webserver::UartOversampling;
using webserver::UartParity;
using webserver::UartPort;
using webserver::UartStopBits;
using webserver::UartWordLength;

bool Accept(void* /*context*/, const UartConfig& config) noexcept {
  logging::Info("sim", "port {} applied: {} baud", config.port_id,
                config.baud_rate);
  return true;
}

// A pretend bridge: accepts the settings and reports a listening port,
// so the bridge part of the UI can be tried on the PC.
bool ApplyBridge(void* /*context*/, std::uint8_t port_id,
                 const webserver::UartBridgeSettings& settings) noexcept {
  logging::Info("sim", "port {} bridge {} on TCP {}", port_id,
                settings.enabled ? "on" : "off", settings.tcp_port);
  return true;
}

bool BridgeStatus(void* /*context*/, std::uint8_t port_id,
                  webserver::UartBridgeStatus& status) noexcept {
  const webserver::UartPort& port = SimulatedPorts()[port_id - 1];
  status = {.listening = port.bridge.enabled,
            .uart_to_net_bytes = 1234U * port_id,
            .net_to_uart_bytes = 567U * port_id};
  return true;
}

constexpr webserver::UartBridgeDriver kSimulatedBridge{
    .apply = &ApplyBridge, .status = &BridgeStatus, .context = nullptr};

// Like real hardware that cannot reach every rate its range suggests.
bool AcceptUpTo1M(void* context, const UartConfig& config) noexcept {
  return config.baud_rate <= 1'000'000 && Accept(context, config);
}

// What the STM32H7 USART driver offers (port with RTS/CTS pins).
constexpr UartCapabilities kStm32Like{
    .min_baud_rate = 1'200,
    .max_baud_rate = 12'500'000,
    .word_lengths = {UartWordLength::kBits7, UartWordLength::kBits8},
    .stop_bits = {UartStopBits::kOne, UartStopBits::kOneAndHalf,
                  UartStopBits::kTwo},
    .parities = {UartParity::kNone, UartParity::kEven, UartParity::kOdd},
    .flow_controls = {UartFlowControl::kNone, UartFlowControl::kRts,
                      UartFlowControl::kCts, UartFlowControl::kRtsCts},
    .oversampling = {UartOversampling::kBy16, UartOversampling::kBy8},
    .fifo = true,
    .tx_rx_swap = true,
};

// What Zephyr's generic uart_configure() offers.
constexpr UartCapabilities kZephyrLike{
    .min_baud_rate = 1'200,
    .max_baud_rate = 4'000'000,
    .word_lengths = {UartWordLength::kBits7, UartWordLength::kBits8,
                     UartWordLength::kBits9},
    .stop_bits = {UartStopBits::kHalf, UartStopBits::kOne,
                  UartStopBits::kOneAndHalf, UartStopBits::kTwo},
    .parities = {UartParity::kNone, UartParity::kEven, UartParity::kOdd},
    .flow_controls = {UartFlowControl::kNone, UartFlowControl::kRtsCts},
    .oversampling = {UartOversampling::kBy16},
};

// A minimal port: 8 data bits only, no flow control.
constexpr UartCapabilities kMinimal{
    .min_baud_rate = 9'600,
    .max_baud_rate = 2'000'000,
    .parities = {UartParity::kNone, UartParity::kEven, UartParity::kOdd},
};

std::array<UartPort, 3> ports{{
    {.id = 1,
     .name = "SIM1",
     .description = "Like an STM32H7 USART with RTS/CTS",
     .capabilities = kStm32Like,
     .driver = {&Accept, nullptr},
     .config = {.port_id = 1, .fifo_enabled = true},
     .bridge_driver = kSimulatedBridge,
     .bridge = {.enabled = true, .tcp_port = 4001}},
    {.id = 2,
     .name = "SIM2",
     .description = "Like a Zephyr UART (uart_configure)",
     .capabilities = kZephyrLike,
     .driver = {&Accept, nullptr},
     .config = {.port_id = 2},
     .bridge_driver = kSimulatedBridge},
    {.id = 3,
     .name = "SIM3",
     .description = "Minimal port, refuses more than 1 Mbit/s",
     .capabilities = kMinimal,
     .driver = {&AcceptUpTo1M, nullptr},
     .config = {.port_id = 3, .baud_rate = 9'600}},
}};

}  // namespace

std::span<UartPort> SimulatedPorts() noexcept { return ports; }

}  // namespace host
