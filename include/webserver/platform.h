#ifndef WEBSERVER_INCLUDE_WEBSERVER_PLATFORM_
#define WEBSERVER_INCLUDE_WEBSERVER_PLATFORM_

#include <cstdint>
#include <span>

namespace webserver {

// What the web server needs from the platform besides the network stack.
struct Platform {
  // Monotonic milliseconds; may wrap (only differences are used).
  std::uint32_t (*now_ms)() noexcept = nullptr;
  // Fills `bytes` from a cryptographically secure source (session tokens).
  // False if none is available right now.
  bool (*random)(std::span<std::uint8_t> bytes) noexcept = nullptr;
};

}  // namespace webserver

#endif  // WEBSERVER_INCLUDE_WEBSERVER_PLATFORM_
