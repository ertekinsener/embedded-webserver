#ifndef WEBSERVER_INCLUDE_WEBSERVER_SETTINGS_SETTINGS_API_
#define WEBSERVER_INCLUDE_WEBSERVER_SETTINGS_SETTINGS_API_

#include "webserver/http_types.h"
#include "webserver/settings/settings_store.h"

namespace webserver {

// /api/settings/* handlers.
class SettingsApi {
 public:
  explicit SettingsApi(SettingsStore& store) noexcept : store_(store) {}

  // POST /api/settings/reset: 204, the saved settings and password are
  // deleted (the running ones stay until the next start); 501 the device
  // cannot save settings, 500 deleting failed.
  void Reset(const HttpRequest& request, HttpResponse& response) noexcept;

 private:
  SettingsStore& store_;
};

}  // namespace webserver

#endif  // WEBSERVER_INCLUDE_WEBSERVER_SETTINGS_SETTINGS_API_
