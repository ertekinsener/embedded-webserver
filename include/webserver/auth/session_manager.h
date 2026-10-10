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
// key (generated at build time, see cmake/credentials.cmake).
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

// Login and bearer-token sessions, all in RAM (a reboot logs everyone out).
// At most kMaxSessions at a time; a new login evicts the least recently used
// one. Not thread safe: call from the HTTP server thread only.
class SessionManager {
 public:
  static constexpr std::size_t kMaxSessions = 4;
  static constexpr std::size_t kTokenBytes = 16;
  static constexpr std::size_t kTokenLength = 2 * kTokenBytes;  // hex
  using Token = std::array<char, kTokenLength>;

  SessionManager(const Credentials& credentials, const Platform& platform,
                 const SessionPolicy& policy = {}) noexcept
      : credentials_(credentials), platform_(platform), policy_(policy) {}

  std::expected<Token, LoginError> Login(std::string_view username,
                                         std::string_view password) noexcept;

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

  bool CheckPassword(std::string_view username,
                     std::string_view password) const noexcept;
  bool Expired(const Session& session, std::uint32_t now) const noexcept;
  Session* Find(std::string_view token) noexcept;
  Session& FreeSlot(std::uint32_t now) noexcept;

  Credentials credentials_;
  Platform platform_;
  SessionPolicy policy_;
  std::array<Session, kMaxSessions> sessions_{};
  std::uint32_t failures_ = 0;
  std::uint32_t locked_since_ms_ = 0;
};

// The token of an "Authorization: Bearer <token>" header value; empty if
// the scheme is not Bearer.
std::string_view BearerToken(std::string_view authorization) noexcept;

}  // namespace webserver::auth

#endif  // WEBSERVER_INCLUDE_WEBSERVER_AUTH_SESSION_MANAGER_
