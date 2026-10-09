#ifndef APP_INCLUDE_WEBSERVER_UART_CONFIG_
#define APP_INCLUDE_WEBSERVER_UART_CONFIG_

#include <cstdint>

namespace webserver {

// UART settings as the web UI sends them. The enumerator values match the
// enums in web/schema/uart_config.fbs, which are decoded by casting.

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
  std::uint8_t port_id;
  std::uint32_t baud_rate;
  UartWordLength word_length;
  UartStopBits stop_bits;
  UartParity parity;
  UartFlowControl flow_control;
  UartOversampling oversampling;
  bool fifo_enabled;
  bool tx_rx_swap;
};

}  // namespace webserver

#endif  // APP_INCLUDE_WEBSERVER_UART_CONFIG_
