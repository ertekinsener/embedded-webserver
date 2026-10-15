#include "webserver/system/system_api.h"

#include <cstddef>
#include <cstdint>

#include "system_generated.h"
#include "webserver/flatbuffer_io.h"

namespace webserver {
namespace {

// Upper bound of the encoded size besides the four strings.
constexpr std::size_t kInfoBytes = 192;

}  // namespace

void SystemApi::Info(const HttpRequest& /*request*/,
                     HttpResponse& response) noexcept {
  const SystemDescription& d = description_;
  if (kInfoBytes + d.device.size() + d.platform.size() +
          d.firmware_version.size() + d.build_date.size() >
      response.buffer().size()) {
    response.SetText(HttpStatus::kInternalError, "System info too large");
    return;
  }
  NetworkInfo network;
  const bool network_up = d.network != nullptr && d.network(network);
  const std::size_t ip_size = network_up ? network.ipv4_address.size() : 0;
  const std::size_t mac_size = network_up ? network.mac_address.size() : 0;

  ResponseBuilder out(response);
  auto& b = out.builder;
  const auto device = b.CreateString(d.device.data(), d.device.size());
  const auto platform = b.CreateString(d.platform.data(), d.platform.size());
  const auto version =
      b.CreateString(d.firmware_version.data(), d.firmware_version.size());
  const auto build_date =
      b.CreateString(d.build_date.data(), d.build_date.size());
  const auto address = b.CreateVector(network.ipv4_address.data(), ip_size);
  const auto netmask = b.CreateVector(network.ipv4_netmask.data(), ip_size);
  const auto mac = b.CreateVector(network.mac_address.data(), mac_size);
  b.Finish(api::CreateSystemInfo(
      b, device, platform, version, build_date,
      d.uptime_s != nullptr ? d.uptime_s() : 0, address, netmask, mac,
      settings_saved_));
  out.Send(HttpStatus::kOk);
}

}  // namespace webserver
