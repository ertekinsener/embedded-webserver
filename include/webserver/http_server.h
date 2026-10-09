#ifndef APP_INCLUDE_WEBSERVER_HTTP_SERVER_
#define APP_INCLUDE_WEBSERVER_HTTP_SERVER_

#include "webserver/api_router.h"
#include "webserver/http_types.h"
#include "webserver/resource_registry.h"

namespace webserver {

// Routes "/api/..." to the ApiRouter and everything else to the embedded
// static files (GET and HEAD only). Independent of NetX: the adapter
// (netx_duo_adapter.h) translates requests and responses.
class HttpServer {
 public:
  HttpServer(const ResourceRegistry& resources, const ApiRouter& api) noexcept
      : resources_(resources), api_(api) {}

  void HandleRequest(const HttpRequest& request,
                     HttpResponse& response) const noexcept;

 private:
  const ResourceRegistry& resources_;
  const ApiRouter& api_;
};

}  // namespace webserver

#endif  // APP_INCLUDE_WEBSERVER_HTTP_SERVER_
