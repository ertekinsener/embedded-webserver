#ifndef WEBSERVER_INCLUDE_WEBSERVER_HTTP_SERVER_
#define WEBSERVER_INCLUDE_WEBSERVER_HTTP_SERVER_

#include "webserver/api_router.h"
#include "webserver/http_types.h"
#include "webserver/resource_registry.h"

namespace webserver {

// Routes "/api/..." to the ApiRouter and everything else to the embedded
// static files (GET and HEAD only). Independent of the network stack: an
// adapter (adapters/*) translates requests and responses.
class HttpServer {
 public:
  HttpServer(const ResourceRegistry& resources, const ApiRouter& api) noexcept
      : resources_(resources), api_(api) {}

  // `response` must be Reset() (as after construction).
  void HandleRequest(const HttpRequest& request,
                     HttpResponse& response) const noexcept;

 private:
  void ServeFile(const HttpRequest& request,
                 HttpResponse& response) const noexcept;

  const ResourceRegistry& resources_;
  const ApiRouter& api_;
};

}  // namespace webserver

#endif  // WEBSERVER_INCLUDE_WEBSERVER_HTTP_SERVER_
