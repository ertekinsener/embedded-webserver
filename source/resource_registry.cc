#include "webserver/resource_registry.h"

#include <string_view>

namespace webserver {

const WebResource* ResourceRegistry::Find(
    std::string_view path) const noexcept {
  if (path == "/") {
    path = "/index.html";
  }
  for (const WebResource& resource : resources_) {
    if (resource.path == path) {
      return &resource;
    }
  }
  return nullptr;
}

}  // namespace webserver
