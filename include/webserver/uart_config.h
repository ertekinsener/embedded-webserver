#ifndef APP_INCLUDE_WEBSERVER_UART_CONFIG_
#define APP_INCLUDE_WEBSERVER_UART_CONFIG_

#include <cstdint>
namespace webserver {

enum class UartWordLength : std::uint8_t { Bits7 = 0, Bits8 = 1, Bits9 = 2 };

enum class UartStopBits : std::uint8_t {
  Bits1 = 0,
  Bits0_5 = 1,
  Bits2 = 2,
  Bits1_5 = 3
};

enum class UartParity : std::uint8_t { None = 0, Even = 1, Odd = 2 };

enum class UartFlowControl : std::uint8_t {
  None = 0,
  RTS = 1,
  CTS = 2,
  RTS_CTS = 3
};

enum class UartOversampling : std::uint8_t { Over16 = 0, Over8 = 1 };

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




#endif /* APP_INCLUDE_WEBSERVER_UART_CONFIG_ */
