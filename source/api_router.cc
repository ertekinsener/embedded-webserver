#include "webserver/api_router.h"

#include "logging/logger.h"

namespace webserver {

void ApiRouter::Handle(const HttpRequest& request,
                       HttpResponse& response) const noexcept {
  for (const Route& route : routes_) {
    if (route.method == request.method && route.path == request.path) {
      route.handler(request, response);
      return;
    }
  }
  logging::Warning("web", "no API route for {} (method {})", request.path,
                   request.method);
  response.SetText(HttpStatus::kNotFound, "Not Found");
}

}  // namespace webserver
