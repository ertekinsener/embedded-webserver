#ifndef APP_INCLUDE_WEBSERVER_HTTP_SERVER_
#define APP_INCLUDE_WEBSERVER_HTTP_SERVER_


#include "webserver/api_router.h"
#include "webserver/http_types.h"
#include "webserver/resource_registry.h"

namespace webserver {

class HttpServer {
 public:
  HttpServer(ResourceRegistry& resources, ApiRouter& api) noexcept;
  void handle_request(const HttpRequest& request,
                      HttpResponse& response) noexcept;

 private:
  ResourceRegistry& resources_;
  ApiRouter& api_;
};

}  // namespace webserver


#endif /* APP_INCLUDE_WEBSERVER_HTTP_SERVER_ */
