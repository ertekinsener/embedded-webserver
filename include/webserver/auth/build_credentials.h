#ifndef WEBSERVER_INCLUDE_WEBSERVER_AUTH_BUILD_CREDENTIALS_
#define WEBSERVER_INCLUDE_WEBSERVER_AUTH_BUILD_CREDENTIALS_

#include "webserver/auth/session_manager.h"

namespace webserver::auth {

// The login configured at build time (WEBSERVER_ADMIN_USER and
// WEBSERVER_ADMIN_PASSWORD, see webserver.cmake). Generated source.
Credentials BuildCredentials() noexcept;

}  // namespace webserver::auth

#endif  // WEBSERVER_INCLUDE_WEBSERVER_AUTH_BUILD_CREDENTIALS_
