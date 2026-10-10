#include "webserver/api_router.h"

#include "logging/logger.h"

namespace webserver {

void ApiRouter::Handle(const HttpRequest& request,
                       HttpResponse& response) const noexcept {
  bool path_known = false;
  for (const Route& route : routes_) {
    if (route.path != request.path) {
      continue;
    }
    path_known = true;
    if (route.method != request.method) {
      continue;
    }
    if (route.access == Access::kSession && !Authorized(request)) {
      response.SetText(HttpStatus::kUnauthorized, "Login required");
      return;
    }
    route.handler(request, response);
    return;
  }
  if (path_known) {
    response.SetText(HttpStatus::kMethodNotAllowed, "Method Not Allowed");
    return;
  }
  logging::Warning("web", "no API route for {}", request.path);
  response.SetText(HttpStatus::kNotFound, "Not Found");
}

}  // namespace webserver
