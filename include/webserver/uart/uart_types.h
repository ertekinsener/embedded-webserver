#ifndef WEBSERVER_INCLUDE_WEBSERVER_UART_UART_TYPES_
#define WEBSERVER_INCLUDE_WEBSERVER_UART_UART_TYPES_

#include <cstdint>
#include <initializer_list>
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

// One port offered by the web UI, owned by the platform's application code.
struct UartPort {
  std::uint8_t id;
  std::string_view name;         // e.g. "USART3"
  std::string_view description;  // e.g. "ST-LINK VCP, PD8/PD9"
  UartCapabilities capabilities;
  UartPortDriver driver;
  // The settings the port runs with; updated by UartService.
  UartConfig config;
};

}  // namespace webserver

#endif  // WEBSERVER_INCLUDE_WEBSERVER_UART_UART_TYPES_
