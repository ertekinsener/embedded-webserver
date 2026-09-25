#ifndef APP_INCLUDE_WEBSERVER_API_ROUTER_
#define APP_INCLUDE_WEBSERVER_API_ROUTER_

#include <span>
#include <string_view>

#include "webserver/http_types.h"

namespace webserver {

class ApiRouter {
 public:
  struct Handler {
    using Function = void (*)(void* context, const HttpRequest& request,
                              HttpResponse& response) noexcept;

    Function function;
    void* context;

    void operator()(const HttpRequest& request,
                    HttpResponse& response) const noexcept {
      function(context, request, response);
    }
  };
  struct Route {
    HttpMethod method;
    std::string_view path;
    Handler handler;
  };

  explicit ApiRouter(std::span<const Route> routes) noexcept
      : routes_(routes) {}

  void handle(const HttpRequest& request,
              HttpResponse& response) const noexcept;

 private:
  std::span<const Route> routes_;
};

}  // namespace webserver


#endif /* APP_INCLUDE_WEBSERVER_API_ROUTER_ */
