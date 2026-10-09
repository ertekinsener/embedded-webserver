#ifndef APP_INCLUDE_WEBSERVER_RESOURCE_REGISTRY_
#define APP_INCLUDE_WEBSERVER_RESOURCE_REGISTRY_

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace webserver {

// One static file embedded into the firmware (see web/WebResources.cmake).
struct WebResource {
  std::string_view path;
  std::string_view content_type;
  const std::uint8_t* data;
  std::size_t size;
};

class ResourceRegistry {
 public:
  explicit ResourceRegistry(std::span<const WebResource> resources) noexcept
      : resources_(resources) {}

  // "/" maps to "/index.html". Returns nullptr if there is no such file.
  const WebResource* Find(std::string_view path) const noexcept;

 private:
  std::span<const WebResource> resources_;
};

}  // namespace webserver

#endif  // APP_INCLUDE_WEBSERVER_RESOURCE_REGISTRY_
