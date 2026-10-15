#ifndef WEBSERVER_INCLUDE_WEBSERVER_UART_UART_TYPES_
#define WEBSERVER_INCLUDE_WEBSERVER_UART_UART_TYPES_

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <utility>

namespace webserver {

// Platform-independent UART settings as the web UI sends them. The
// enumerator values match the enums in schema/uart.fbs, which are decoded by
// casting (and range-checked by UartService).

enum class UartWordLength : std::uint8_t { kBits7 = 0, kBits8 = 1, kBits9 = 2 };

enum class UartStopBits : std::uint8_t {
  kOne = 0,
  kHalf = 1,
  kTwo = 2,
  kOneAndHalf = 3,
};

enum class UartParity : std::uint8_t { kNone = 0, kEven = 1, kOdd = 2 };

enum class UartFlowControl : std::uint8_t {
  kNone = 0,
  kRts = 1,
  kCts = 2,
  kRtsCts = 3,
};

enum class UartOversampling : std::uint8_t { kBy16 = 0, kBy8 = 1 };

struct UartConfig {
  std::uint8_t port_id = 0;
  std::uint32_t baud_rate = 115'200;
  UartWordLength word_length = UartWordLength::kBits8;
  UartStopBits stop_bits = UartStopBits::kOne;
  UartParity parity = UartParity::kNone;
  UartFlowControl flow_control = UartFlowControl::kNone;
  UartOversampling oversampling = UartOversampling::kBy16;
  bool fifo_enabled = false;
  bool tx_rx_swap = false;
};

// A set of enumerators of a small enum (values < 32).
template <typename Enum>
class EnumSet {
 public:
  constexpr EnumSet() noexcept = default;
  constexpr EnumSet(std::initializer_list<Enum> values) noexcept {
    for (Enum value : values) {
      bits_ |= Bit(value);
    }
  }

  constexpr bool Contains(Enum value) const noexcept {
    return std::to_underlying(value) < 32 && (bits_ & Bit(value)) != 0;
  }
  constexpr void Insert(Enum value) noexcept { bits_ |= Bit(value); }
  constexpr bool empty() const noexcept { return bits_ == 0; }
  constexpr std::uint32_t bits() const noexcept { return bits_; }

 private:
  static constexpr std::uint32_t Bit(Enum value) noexcept {
    return std::uint32_t{1} << std::to_underlying(value);
  }

  std::uint32_t bits_ = 0;
};

// What a port supports; the web UI offers only these, and UartService
// rejects anything else before the port sees it. A port can still refuse a
// supported combination (e.g. a baud rate its clock cannot produce).
struct UartCapabilities {
  std::uint32_t min_baud_rate = 1'200;
  std::uint32_t max_baud_rate = 115'200;
  EnumSet<UartWordLength> word_lengths{UartWordLength::kBits8};
  EnumSet<UartStopBits> stop_bits{UartStopBits::kOne};
  EnumSet<UartParity> parities{UartParity::kNone};
  EnumSet<UartFlowControl> flow_controls{UartFlowControl::kNone};
  EnumSet<UartOversampling> oversampling{UartOversampling::kBy16};
  bool fifo = false;        // fifo_enabled may be set.
  bool tx_rx_swap = false;  // tx_rx_swap may be set.
};

constexpr bool Supports(const UartCapabilities& capabilities,
                        const UartConfig& config) noexcept {
  return config.baud_rate >= capabilities.min_baud_rate &&
         config.baud_rate <= capabilities.max_baud_rate &&
         capabilities.word_lengths.Contains(config.word_length) &&
         capabilities.stop_bits.Contains(config.stop_bits) &&
         capabilities.parities.Contains(config.parity) &&
         capabilities.flow_controls.Contains(config.flow_control) &&
         capabilities.oversampling.Contains(config.oversampling) &&
         (capabilities.fifo || !config.fifo_enabled) &&
         (capabilities.tx_rx_swap || !config.tx_rx_swap);
}

// Applies settings to one port's hardware; false if it refuses them. Called
// from the HTTP server thread.
struct UartPortDriver {
  using Apply = bool (*)(void* context, const UartConfig& config) noexcept;

  Apply apply = nullptr;
  void* context = nullptr;
};

// Ethernet bridge of a port (docs/bridge/README.md). The enumerator values
// match BridgeMode in schema/uart.fbs.
enum class UartBridgeMode : std::uint8_t {
  kTcpServer = 0,  // Raw TCP server on local_port.
  kRfc2217 = 1,    // Telnet with COM Port Control (RFC 2217) on local_port.
  kTcpClient = 2,  // Connects to remote_address:remote_port.
  kUdp = 3,        // Datagrams on local_port; sends to remote_address:
                   // remote_port, or to the last sender if that is unset.
};

struct UartBridgeSettings {
  bool enabled = false;
  UartBridgeMode mode = UartBridgeMode::kTcpServer;
  std::uint16_t local_port = 0;
  std::array<std::uint8_t, 4> remote_address{};
  std::uint16_t remote_port = 0;
  // 0: send UART data when the line goes idle; else this long after the
  // last byte, merging frames.
  std::uint16_t idle_gap_ms = 0;
  std::uint16_t max_frame = 1024;  // Bytes per TCP segment / datagram.
  // Also send as soon as this byte arrives (and not on idle when
  // idle_gap_ms is 0).
  bool use_delimiter = false;
  std::uint8_t delimiter = '\n';
};

// Limits UartService checks; ports below 1024 (e.g. HTTP) are not offered.
inline constexpr std::uint16_t kMinBridgeLocalPort = 1024;
inline constexpr std::uint16_t kMaxBridgeIdleGapMs = 1000;
inline constexpr std::uint16_t kMaxBridgeFrame = 1460;

// `listening`: bound and waiting (servers, UDP) or connecting (client).
// `connected`: a peer is attached (the TCP client, the server's client, or
// the UDP peer data is sent to); its address is client_address/port.
struct UartBridgeStatus {
  bool listening = false;
  bool connected = false;
  std::array<std::uint8_t, 4> client_address{};
  std::uint16_t client_port = 0;
  std::uint32_t uart_to_net_bytes = 0;
  std::uint32_t net_to_uart_bytes = 0;
  std::uint32_t dropped_bytes = 0;  // UART data with no client to take it.
  std::uint32_t uart_errors = 0;
};

// The platform's bridge for a port; `apply` == nullptr: the port has none.
// Called from the HTTP server thread.
struct UartBridgeDriver {
  using Apply = bool (*)(void* context, std::uint8_t port_id,
                         const UartBridgeSettings& settings) noexcept;
  using GetStatus = bool (*)(void* context, std::uint8_t port_id,
                             UartBridgeStatus& status) noexcept;

  Apply apply = nullptr;
  GetStatus status = nullptr;
  void* context = nullptr;
};

// Most ports the web UI lists (and the saved settings hold).
inline constexpr std::size_t kMaxUartPorts = 16;

// One port offered by the web UI, owned by the platform's application code.
struct UartPort {
  std::uint8_t id;
  std::string_view name;         // e.g. "USART3"
  std::string_view description;  // e.g. "ST-LINK VCP, PD8/PD9"
  UartCapabilities capabilities;
  UartPortDriver driver;
  // The settings the port runs with; updated by UartService.
  UartConfig config;
  UartBridgeDriver bridge_driver{};
  UartBridgeSettings bridge{};  // Updated by UartService.
  // Settings last applied from the web UI (or restored at start), which
  // SettingsStore saves; an RFC 2217 client changes only `config`. nullopt:
  // never changed, the platform's default applies.
  std::optional<UartConfig> saved_config{};
};

}  // namespace webserver

#endif  // WEBSERVER_INCLUDE_WEBSERVER_UART_UART_TYPES_
