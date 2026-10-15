#ifndef WEBSERVER_INCLUDE_WEBSERVER_SETTINGS_SETTINGS_STORE_
#define WEBSERVER_INCLUDE_WEBSERVER_SETTINGS_SETTINGS_STORE_

#include <cstddef>
#include <cstdint>
#include <span>

#include "webserver/auth/session_manager.h"
#include "webserver/uart/uart_service.h"

namespace webserver {

// Where the platform keeps the saved settings (a flash log on the CM7, a
// file on the PC). Called from the HTTP server thread. Without `save`,
// settings and password live in RAM only.
struct SettingsStorage {
  // Copies the saved settings into `buffer`; their size, 0 if there are
  // none or they do not fit.
  std::size_t (*load)(void* context,
                      std::span<std::uint8_t> buffer) noexcept = nullptr;
  bool (*save)(void* context,
               std::span<const std::uint8_t> data) noexcept = nullptr;
  // Deletes them: platform defaults and the build-time password from the
  // next start on.
  bool (*erase)(void* context) noexcept = nullptr;
  void* context = nullptr;
};

// Saves what the web UI changed (port settings, bridges, a changed
// password) as a FlatBuffer (schema/settings.fbs) and applies it again at
// the next start. Not thread safe: HTTP server thread (or before it runs).
class SettingsStore {
 public:
  // Largest encoding (kMaxUartPorts ports with every field set; checked by
  // the host tests); the scratch buffers must be this large.
  static constexpr std::size_t kMaxSize = 1536;

  SettingsStore(UartService& uart, auth::SessionManager& sessions,
                const SettingsStorage& storage) noexcept
      : uart_(uart), sessions_(sessions), storage_(storage) {}

  SettingsStore(const SettingsStore&) = delete;
  SettingsStore& operator=(const SettingsStore&) = delete;

  bool available() const noexcept { return storage_.save != nullptr; }

  // Applies the saved settings through UartService and SessionManager, once
  // the ports and their bridges take settings. Settings a port refuses now
  // (e.g. after a firmware update) are skipped. False if nothing valid was
  // saved.
  bool Restore(std::span<std::uint8_t> scratch) noexcept;

  // Saves the current settings. `scratch` may be the response buffer of the
  // request that changed them. False without storage or on a write error.
  bool Save(std::span<std::uint8_t> scratch) noexcept;

  // Deletes the saved settings (see SettingsStorage::erase).
  bool Erase() noexcept;

 private:
  UartService& uart_;
  auth::SessionManager& sessions_;
  SettingsStorage storage_;
};

}  // namespace webserver

#endif  // WEBSERVER_INCLUDE_WEBSERVER_SETTINGS_SETTINGS_STORE_
