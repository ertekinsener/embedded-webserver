#ifndef APP_INCLUDE_WEBSERVER_RESOURCE_REGISTRY_
#define APP_INCLUDE_WEBSERVER_RESOURCE_REGISTRY_

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace webserver {

struct WebResource {
  std::string_view path;
  std::string_view content_type;
  const std::uint8_t* data;
  std::size_t size;
};

class ResourceRegistry {
 public:
  ResourceRegistry(const WebResource* resources, std::size_t count) noexcept;
  const WebResource* find(std::string_view path) const noexcept;

 private:
  const WebResource* resources_;
  std::size_t count_;
};

}  // namespace webserver


#endif /* APP_INCLUDE_WEBSERVER_RESOURCE_REGISTRY_ */
