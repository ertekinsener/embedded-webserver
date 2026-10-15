#ifndef WEBSERVER_INCLUDE_WEBSERVER_AUTH_SESSION_MANAGER_
#define WEBSERVER_INCLUDE_WEBSERVER_AUTH_SESSION_MANAGER_

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>

#include "webserver/platform.h"

namespace webserver::auth {

// The one account of the web UI. The password is stored only as a PBKDF2
// key (generated at build time, see webserver.cmake, or set through
// ChangePassword()).
struct Credentials {
  std::string_view username;
  std::span<const std::uint8_t> salt;
  std::uint32_t iterations;
  std::span<const std::uint8_t> derived_key;
};

struct SessionPolicy {
  std::uint32_t idle_timeout_ms = 15 * 60 * 1000;
  // After this many failed logins in a row, logins are refused for
  // `lockout_ms`.
  std::uint32_t max_failures = 5;
  std::uint32_t lockout_ms = 30 * 1000;
};

enum class LoginError : std::uint8_t {
  kInvalidCredentials,
  kLockedOut,
  kNoRandom,  // The platform could not provide a token.
};

enum class PasswordError : std::uint8_t {
  kWrongPassword,    // The current password; counts as a failed login.
  kLockedOut,
  kInvalidPassword,  // The new one is too short or too long.
  kNoRandom,         // The platform could not provide a salt.
};

// Limits of a password set from the web UI (bytes of UTF-8).
inline constexpr std::size_t kMinPasswordLength = 8;
inline constexpr std::size_t kMaxPasswordLength = 64;

// Login and bearer-token sessions, all in RAM (a reboot logs everyone out).
// At most kMaxSessions at a time; a new login evicts the least recently used
// one. Not thread safe: call from the HTTP server thread only.
class SessionManager {
 public:
  static constexpr std::size_t kMaxSessions = 4;
  static constexpr std::size_t kTokenBytes = 16;
  static constexpr std::size_t kTokenLength = 2 * kTokenBytes;  // hex
  static constexpr std::size_t kMaxSaltSize = 32;
  static constexpr std::size_t kMinKeySize = 16;
  static constexpr std::size_t kMaxKeySize = 64;
  static constexpr std::uint32_t kMaxIterations = 1'000'000;
  using Token = std::array<char, kTokenLength>;

  // Copies the salt and key of `credentials` (at most kMaxSaltSize and
  // kMaxKeySize bytes); the user name must outlive the manager.
  SessionManager(const Credentials& credentials, const Platform& platform,
                 const SessionPolicy& policy = {}) noexcept;

  SessionManager(const SessionManager&) = delete;
  SessionManager& operator=(const SessionManager&) = delete;

  std::expected<Token, LoginError> Login(std::string_view username,
                                         std::string_view password) noexcept;

  // Replaces the password by `new_password` (a new random salt, the current
  // iterations and key size) once `current` is verified. Every session but
  // the one of `keep_token` ends.
  std::expected<void, PasswordError> ChangePassword(
      std::string_view current, std::string_view new_password,
      std::string_view keep_token) noexcept;

  // Uses another PBKDF2 key for the password (saved settings); the user name
  // stays. False, and no change, if a size or the iterations are out of
  // range.
  bool SetPasswordKey(std::span<const std::uint8_t> salt,
                      std::uint32_t iterations,
                      std::span<const std::uint8_t> derived_key) noexcept;

  // The current password key; the spans point into the manager.
  const Credentials& credentials() const noexcept { return credentials_; }
  // True once the key is no longer the one passed to the constructor.
  bool password_changed() const noexcept { return password_changed_; }

  // True if `token` belongs to a live session; that session's idle timer
  // restarts.
  bool Validate(std::string_view token) noexcept;

  // Ends the session of `token`, if any.
  void Logout(std::string_view token) noexcept;

  const SessionPolicy& policy() const noexcept { return policy_; }

 private:
  struct Session {
    Token token{};
    std::uint32_t last_used_ms = 0;
    bool active = false;
  };

  bool CheckPassword(std::string_view password) const noexcept;
  bool LockedOut(std::uint32_t now) noexcept;
  void CountFailure(std::uint32_t now) noexcept;
  bool Expired(const Session& session, std::uint32_t now) const noexcept;
  Session* Find(std::string_view token) noexcept;
  Session& FreeSlot(std::uint32_t now) noexcept;

  std::array<std::uint8_t, kMaxSaltSize> salt_{};
  std::array<std::uint8_t, kMaxKeySize> key_{};
  Credentials credentials_;  // salt and derived_key view salt_ and key_.
  Platform platform_;
  SessionPolicy policy_;
  std::array<Session, kMaxSessions> sessions_{};
  std::uint32_t failures_ = 0;
  std::uint32_t locked_since_ms_ = 0;
  bool password_changed_ = false;
};

// The token of an "Authorization: Bearer <token>" header value; empty if
// the scheme is not Bearer.
std::string_view BearerToken(std::string_view authorization) noexcept;

}  // namespace webserver::auth

#endif  // WEBSERVER_INCLUDE_WEBSERVER_AUTH_SESSION_MANAGER_
