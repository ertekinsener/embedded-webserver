#ifndef WEBSERVER_INCLUDE_WEBSERVER_UART_UART_SERVICE_
#define WEBSERVER_INCLUDE_WEBSERVER_UART_UART_SERVICE_

#include <cstdint>
#include <expected>
#include <span>

#include "webserver/uart/uart_types.h"

namespace webserver {

enum class UartError : std::uint8_t {
  kUnknownPort,
  kUnsupported,  // Out of range or not in the port's capabilities.
  kRejected,     // The port's driver refused the settings.
};

// Optional lock of the platform: without one, UartService is used from
// the HTTP server thread only; with one, also from others (e.g. an RFC 2217
// client of the bridge changing the baud rate).
struct UartServiceLock {
  void (*lock)(void* context) noexcept = nullptr;
  void (*unlock)(void* context) noexcept = nullptr;
  void* context = nullptr;
};

// The UART ports of the web UI: checks settings against each port's
// capabilities and hands them to its driver.
class UartService {
 public:
  // Holds the lock while it exists.
  class Guard {
   public:
    explicit Guard(const UartServiceLock& lock) noexcept : lock_(lock) {
      if (lock_.lock != nullptr) {
        lock_.lock(lock_.context);
      }
    }
    ~Guard() {
      if (lock_.unlock != nullptr) {
        lock_.unlock(lock_.context);
      }
    }
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;

   private:
    const UartServiceLock& lock_;
  };

  // Each port's `config` must hold the settings it currently runs with.
  explicit UartService(std::span<UartPort> ports,
                       UartServiceLock lock = {}) noexcept
      : ports_(ports), lock_(lock) {}

  std::expected<void, UartError> Apply(const UartConfig& config) noexcept;

  // kUnsupported also for a port without bridge, values out of range or a
  // local port another enabled bridge uses.
  std::expected<void, UartError> ApplyBridge(
      std::uint8_t port_id, const UartBridgeSettings& settings) noexcept;

  // The settings port `port_id` runs with; false for an unknown port.
  bool GetConfig(std::uint8_t port_id, UartConfig& config) const noexcept;

  // Read ports() only while holding Locked().
  Guard Locked() const noexcept { return Guard(lock_); }
  std::span<const UartPort> ports() const noexcept { return ports_; }

 private:
  UartPort* Find(std::uint8_t id) const noexcept;
  bool LocalPortInUse(const UartPort& except,
                      std::uint16_t local_port) const noexcept;
  static bool Valid(const UartBridgeSettings& settings) noexcept;

  std::span<UartPort> ports_;
  UartServiceLock lock_;
};

}  // namespace webserver

#endif  // WEBSERVER_INCLUDE_WEBSERVER_UART_UART_SERVICE_
