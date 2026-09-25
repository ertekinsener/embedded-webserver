#include "webserver/http_server.h"

namespace webserver {

HttpServer::HttpServer(
    ResourceRegistry& resources, ApiRouter& api) noexcept
    : resources_(resources), api_(api) {}

void HttpServer::handle_request(const HttpRequest& request,
                                HttpResponse& response) noexcept {
  // API requests
  if (request.path.rfind("/api/", 0) == 0) {
    api_.handle(request, response);
    return;
  }
  // Static resources only support GET and HEAD.
  if (request.method != HttpMethod::Get && request.method != HttpMethod::Head) {
    response.status = 405;
    response.set_body("Method Not Allowed", 18, "text/plain");
    return;
  }
  const WebResource* resource = resources_.find(request.path);
  if (resource == nullptr) {
    response.status = 404;
    response.set_body("Not Found", 9, "text/plain");

    return;
  }
  response.status = 200;
  if (request.method == HttpMethod::Head) {
    response.set_owned_body(0, resource->content_type);
    return;
  }
  response.set_body(reinterpret_cast<const char*>(resource->data),
                    resource->size, resource->content_type);
}

}  // namespace webserver
