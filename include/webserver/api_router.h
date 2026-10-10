#ifndef WEBSERVER_INCLUDE_WEBSERVER_API_ROUTER_
#define WEBSERVER_INCLUDE_WEBSERVER_API_ROUTER_

#include <span>
#include <string_view>

#include "webserver/http_types.h"

namespace webserver {

// Dispatches "/api/..." requests by exact path and method. Routes that need
// a session are checked with the Authorizer first (401 if it says no).
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

  enum class Access : bool { kPublic, kSession };

  struct Route {
    HttpMethod method;
    std::string_view path;
    Access access;
    Handler handler;
  };

  struct Authorizer {
    using Function = bool (*)(void* context,
                              const HttpRequest& request) noexcept;

    Function function = nullptr;
    void* context = nullptr;
  };

  ApiRouter(std::span<const Route> routes, Authorizer authorizer) noexcept
      : routes_(routes), authorizer_(authorizer) {}

  // 404 for an unknown path, 405 for a known path with another method.
  void Handle(const HttpRequest& request,
              HttpResponse& response) const noexcept;

 private:
  bool Authorized(const HttpRequest& request) const noexcept {
    return authorizer_.function != nullptr &&
           authorizer_.function(authorizer_.context, request);
  }

  std::span<const Route> routes_;
  Authorizer authorizer_;
};

// A Handler calling `(object.*kMethod)(request, response)`:
//
//   BindHandler<&UartApi::ListPorts>(uart_api)
template <auto kMethod, typename T>
constexpr ApiRouter::Handler BindHandler(T& object) noexcept {
  return {[](void* context, const HttpRequest& request,
             HttpResponse& response) noexcept {
            (static_cast<T*>(context)->*kMethod)(request, response);
          },
          &object};
}

}  // namespace webserver

#endif  // WEBSERVER_INCLUDE_WEBSERVER_API_ROUTER_
