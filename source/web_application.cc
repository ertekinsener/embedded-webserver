#include "webserver/web_application.h"

namespace webserver {

using Access = ApiRouter::Access;

WebApplication::WebApplication(const WebApplicationConfig& config) noexcept
    : resources_(config.resources),
      sessions_(config.credentials, config.platform, config.session_policy),
      uart_service_(config.uart_ports, config.uart_lock),
      settings_(uart_service_, sessions_, config.settings_storage),
      auth_api_(sessions_, &settings_),
      uart_api_(uart_service_, &settings_),
      system_api_(config.system, settings_.available()),
      settings_api_(settings_),
      routes_{{
          {HttpMethod::kPost, "/api/auth/login", Access::kPublic,
           BindHandler<&auth::AuthApi::Login>(auth_api_)},
          {HttpMethod::kPost, "/api/auth/logout", Access::kSession,
           BindHandler<&auth::AuthApi::Logout>(auth_api_)},
          {HttpMethod::kPost, "/api/auth/password", Access::kSession,
           BindHandler<&auth::AuthApi::ChangePassword>(auth_api_)},
          {HttpMethod::kGet, "/api/system/info", Access::kSession,
           BindHandler<&SystemApi::Info>(system_api_)},
          {HttpMethod::kGet, "/api/uart/ports", Access::kSession,
           BindHandler<&UartApi::ListPorts>(uart_api_)},
          {HttpMethod::kPost, "/api/uart/config", Access::kSession,
           BindHandler<&UartApi::SetConfig>(uart_api_)},
          {HttpMethod::kPost, "/api/bridge/config", Access::kSession,
           BindHandler<&UartApi::SetBridge>(uart_api_)},
          {HttpMethod::kPost, "/api/settings/reset", Access::kSession,
           BindHandler<&SettingsApi::Reset>(settings_api_)},
      }},
      router_(routes_,
              {[](void* context, const HttpRequest& request) noexcept {
                 return static_cast<auth::AuthApi*>(context)->Authorize(
                     request);
               },
               &auth_api_}),
      server_(resources_, router_) {}

}  // namespace webserver
