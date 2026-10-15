#include "webserver/settings/settings_api.h"

namespace webserver {

void SettingsApi::Reset(const HttpRequest& /*request*/,
                        HttpResponse& response) noexcept {
  if (!store_.available()) {
    response.SetText(HttpStatus::kNotImplemented,
                     "This device does not save settings");
    return;
  }
  if (!store_.Erase()) {
    response.SetText(HttpStatus::kInternalError,
                     "Deleting the saved settings failed");
    return;
  }
  response.SetEmpty(HttpStatus::kNoContent);
}

}  // namespace webserver
