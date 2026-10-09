#include "webserver/http_server.h"

#include <string_view>

namespace webserver {

void HttpServer::HandleRequest(const HttpRequest& request,
                               HttpResponse& response) const noexcept {
  if (request.path.starts_with("/api/")) {
    api_.Handle(request, response);
    return;
  }
  if (request.method != HttpMethod::kGet &&
      request.method != HttpMethod::kHead) {
    response.SetText(HttpStatus::kMethodNotAllowed, "Method Not Allowed");
    return;
  }
  const WebResource* resource = resources_.Find(request.path);
  if (resource == nullptr) {
    response.SetText(HttpStatus::kNotFound, "Not Found");
    return;
  }
  response.status = HttpStatus::kOk;
  if (request.method == HttpMethod::kHead) {
    response.SetEmptyBody(resource->content_type);
    return;
  }
  response.SetBody({reinterpret_cast<const char*>(resource->data),
                    resource->size},
                   resource->content_type);
}

}  // namespace webserver
