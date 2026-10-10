#include "webserver/http_server.h"

#include <string_view>

namespace webserver {

void HttpServer::HandleRequest(const HttpRequest& request,
                               HttpResponse& response) const noexcept {
  if (request.path.starts_with("/api/")) {
    api_.Handle(request, response);
    return;
  }
  ServeFile(request, response);
}

void HttpServer::ServeFile(const HttpRequest& request,
                           HttpResponse& response) const noexcept {
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
  response.SetBody(HttpStatus::kOk,
                   request.method == HttpMethod::kHead
                       ? std::span<const std::uint8_t>{}
                       : resource->data,
                   resource->content_type);
  response.content_encoding = resource->content_encoding;
  response.cache_control = resource->cache_control;
}

}  // namespace webserver
