#ifndef WEBSERVER_INCLUDE_WEBSERVER_SYSTEM_SYSTEM_API_
#define WEBSERVER_INCLUDE_WEBSERVER_SYSTEM_SYSTEM_API_

#include <array>
#include <cstdint>
#include <string_view>

#include "webserver/http_types.h"

namespace webserver {

struct NetworkInfo {
  std::array<std::uint8_t, 4> ipv4_address{};
  std::array<std::uint8_t, 4> ipv4_netmask{};
  std::array<std::uint8_t, 6> mac_address{};
};

// Static description of the device plus live values from the platform.
struct SystemDescription {
  std::string_view device;
  std::string_view platform;
  std::string_view firmware_version;
  std::string_view build_date;
  std::uint32_t (*uptime_s)() noexcept = nullptr;
  // False if the interface is not up yet.
  bool (*network)(NetworkInfo& info) noexcept = nullptr;
};

// GET /api/system/info (schema/system.fbs).
class SystemApi {
 public:
  explicit SystemApi(const SystemDescription& description) noexcept
      : description_(description) {}

  void Info(const HttpRequest& request, HttpResponse& response) noexcept;

 private:
  SystemDescription description_;
};

}  // namespace webserver

#endif  // WEBSERVER_INCLUDE_WEBSERVER_SYSTEM_SYSTEM_API_
