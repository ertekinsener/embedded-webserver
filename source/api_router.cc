#include "webserver/api_router.h"

namespace webserver {
void ApiRouter::handle(const HttpRequest& request, HttpResponse& response) const noexcept {
  for (const Route& route : routes_) {
    if (route.method == request.method && route.path == request.path) {
      route.handler(request, response);
      return;
    }
  }
  response.status = 404;
  response.set_body("Not Found", 9, "text/plain");
}
}  // namespace webserver