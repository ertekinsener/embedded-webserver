#include "webserver/resource_registry.h"

namespace webserver {

ResourceRegistry::ResourceRegistry(const WebResource* resources,
                                 std::size_t count) noexcept
    : resources_(resources), count_(count) {}

const WebResource* ResourceRegistry::find(std::string_view path) const noexcept {
  if (path == "/") {
    path = "/index.html";
  }

  for (std::size_t i = 0; i < count_; ++i) {
    if (resources_[i].path == path) {
      return &resources_[i];
    }
  }
  return nullptr;
}

}  // namespace webserver
