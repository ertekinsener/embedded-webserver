#ifndef WEBSERVER_INCLUDE_WEBSERVER_AUTH_AUTH_API_
#define WEBSERVER_INCLUDE_WEBSERVER_AUTH_AUTH_API_

#include "webserver/auth/session_manager.h"
#include "webserver/http_types.h"

namespace webserver::auth {

// /api/auth/* handlers (schema/auth.fbs) and the ApiRouter authorizer.
class AuthApi {
 public:
  explicit AuthApi(SessionManager& sessions) noexcept : sessions_(sessions) {}

  // POST /api/auth/login: LoginRequest -> 200 LoginResponse; 401 wrong
  // credentials, 429 locked out after repeated failures.
  void Login(const HttpRequest& request, HttpResponse& response) noexcept;

  // POST /api/auth/logout: 204.
  void Logout(const HttpRequest& request, HttpResponse& response) noexcept;

  // True if the request carries the bearer token of a live session.
  bool Authorize(const HttpRequest& request) noexcept;

 private:
  SessionManager& sessions_;
};

}  // namespace webserver::auth

#endif  // WEBSERVER_INCLUDE_WEBSERVER_AUTH_AUTH_API_
