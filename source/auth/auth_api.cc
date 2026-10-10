#include "webserver/auth/auth_api.h"

#include "auth_generated.h"
#include "logging/logger.h"
#include "webserver/flatbuffer_io.h"

namespace webserver::auth {

void AuthApi::Login(const HttpRequest& request,
                    HttpResponse& response) noexcept {
  const auto* login = DecodeRequest<api::LoginRequest>(request, response);
  if (login == nullptr) {
    return;
  }
  const auto token = sessions_.Login(ToStringView(login->username()),
                                     ToStringView(login->password()));
  if (!token) {
    switch (token.error()) {
      case LoginError::kInvalidCredentials:
        logging::Warning("auth", "login failed");
        response.SetText(HttpStatus::kUnauthorized,
                         "Wrong user name or password");
        return;
      case LoginError::kLockedOut:
        response.SetText(HttpStatus::kTooManyRequests,
                         "Too many failed logins, try again later");
        return;
      case LoginError::kNoRandom:
        logging::Error("auth", "no random numbers for a session token");
        response.SetText(HttpStatus::kServiceUnavailable,
                         "No random source");
        return;
    }
  }
  logging::Info("auth", "user logged in");

  // A 32-character token and a few fields: far below the response buffer.
  ResponseBuilder out(response);
  const auto token_offset =
      out.builder.CreateString(token->data(), token->size());
  out.builder.Finish(api::CreateLoginResponse(
      out.builder, token_offset, sessions_.policy().idle_timeout_ms / 1000));
  out.Send(HttpStatus::kOk);
}

void AuthApi::Logout(const HttpRequest& request,
                     HttpResponse& response) noexcept {
  sessions_.Logout(BearerToken(request.authorization));
  response.SetEmpty(HttpStatus::kNoContent);
}

bool AuthApi::Authorize(const HttpRequest& request) noexcept {
  return sessions_.Validate(BearerToken(request.authorization));
}

}  // namespace webserver::auth
