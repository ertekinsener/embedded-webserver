#include "host_platform.h"

#include <sys/random.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

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

struct SettingsFile {
  const char* path = nullptr;  // nullptr: `memory` only.
  std::vector<std::uint8_t> memory;
};

SettingsFile settings_file;

std::size_t LoadSettings(void* context,
                         std::span<std::uint8_t> buffer) noexcept {
  auto& file = *static_cast<SettingsFile*>(context);
  if (file.path == nullptr) {
    if (file.memory.size() > buffer.size()) {
      return 0;
    }
    std::copy(file.memory.begin(), file.memory.end(), buffer.begin());
    return file.memory.size();
  }
  std::FILE* stream = std::fopen(file.path, "rb");
  if (stream == nullptr) {
    return 0;
  }
  const std::size_t size = std::fread(buffer.data(), 1, buffer.size(), stream);
  const bool complete = std::feof(stream) != 0;
  std::fclose(stream);
  return complete ? size : 0;
}

bool SaveSettings(void* context, std::span<const std::uint8_t> data) noexcept {
  auto& file = *static_cast<SettingsFile*>(context);
  if (file.path == nullptr) {
    file.memory.assign(data.begin(), data.end());
    return true;
  }
  const std::string temporary = std::string(file.path) + ".tmp";
  std::FILE* stream = std::fopen(temporary.c_str(), "wb");
  if (stream == nullptr) {
    return false;
  }
  const bool written =
      std::fwrite(data.data(), 1, data.size(), stream) == data.size();
  return std::fclose(stream) == 0 && written &&
         std::rename(temporary.c_str(), file.path) == 0;
}

bool EraseSettings(void* context) noexcept {
  auto& file = *static_cast<SettingsFile*>(context);
  file.memory.clear();
  return file.path == nullptr || std::remove(file.path) == 0 ||
         access(file.path, F_OK) != 0;
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

webserver::SettingsStorage HostSettingsStorage(const char* path) noexcept {
  settings_file.path = path;
  return {.load = &LoadSettings,
          .save = &SaveSettings,
          .erase = &EraseSettings,
          .context = &settings_file};
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
