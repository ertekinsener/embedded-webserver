#include "host_platform.h"

#include <sys/random.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <span>

#include "logging/logger.h"

namespace host {
namespace {

using Clock = std::chrono::steady_clock;

const Clock::time_point start = Clock::now();

std::uint64_t ClockUs() noexcept {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() -
                                                            start)
          .count());
}

std::uint32_t NowMs() noexcept {
  return static_cast<std::uint32_t>(ClockUs() / 1000);
}

bool Random(std::span<std::uint8_t> bytes) noexcept {
  return getrandom(bytes.data(), bytes.size(), 0) ==
         static_cast<ssize_t>(bytes.size());
}

std::uint32_t UptimeS() noexcept {
  return static_cast<std::uint32_t>(ClockUs() / 1'000'000);
}

bool Network(webserver::NetworkInfo& info) noexcept {
  info.ipv4_address = {127, 0, 0, 1};
  info.ipv4_netmask = {255, 0, 0, 0};
  info.mac_address = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
  return true;
}

}  // namespace

void InitLogging() noexcept {
  logging::Init({
      .core = "host",
      .sink = {[](void*, std::span<const std::uint8_t> line) noexcept {
                 return write(STDOUT_FILENO, line.data(), line.size()) >= 0;
               },
               nullptr},
      .clock_us = &ClockUs,
      .level = logging::Level::kInfo,
  });
}

webserver::Platform HostPlatform() noexcept {
  return {.now_ms = &NowMs, .random = &Random};
}

webserver::SystemDescription HostSystem() noexcept {
  return {
      .device = "PC (simulated)",
      .platform = "POSIX sockets",
      .firmware_version = "host",
      .build_date = __DATE__ " " __TIME__,
      .uptime_s = &UptimeS,
      .network = &Network,
  };
}

}  // namespace host
