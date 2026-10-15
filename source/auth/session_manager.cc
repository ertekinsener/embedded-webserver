#include "webserver/auth/session_manager.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "logging/logger.h"
#include "webserver/auth/sha256.h"
#include "webserver/http_headers.h"

namespace webserver::auth {
namespace {

// Salt of a password set from the web UI.
constexpr std::size_t kNewSaltSize = 16;

std::span<const std::uint8_t> AsBytes(std::string_view text) noexcept {
  return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}

}  // namespace

SessionManager::SessionManager(const Credentials& credentials,
                               const Platform& platform,
                               const SessionPolicy& policy) noexcept
    : credentials_(credentials), platform_(platform), policy_(policy) {
  const std::size_t salt_size = std::min(credentials.salt.size(), kMaxSaltSize);
  const std::size_t key_size =
      std::min(credentials.derived_key.size(), kMaxKeySize);
  std::copy_n(credentials.salt.begin(), salt_size, salt_.begin());
  std::copy_n(credentials.derived_key.begin(), key_size, key_.begin());
  credentials_.salt = std::span(salt_).first(salt_size);
  credentials_.derived_key = std::span(key_).first(key_size);
}

bool SessionManager::CheckPassword(std::string_view password) const noexcept {
  std::array<std::uint8_t, kMaxKeySize> key{};
  const std::span<std::uint8_t> derived =
      std::span(key).first(credentials_.derived_key.size());
  Pbkdf2HmacSha256(AsBytes(password), credentials_.salt,
                   credentials_.iterations, derived);
  return !derived.empty() &&
         ConstantTimeEqual(derived, credentials_.derived_key);
}

bool SessionManager::LockedOut(std::uint32_t now) noexcept {
  if (failures_ >= policy_.max_failures) {
    if (now - locked_since_ms_ < policy_.lockout_ms) {
      return true;
    }
    failures_ = 0;
  }
  return false;
}

void SessionManager::CountFailure(std::uint32_t now) noexcept {
  if (++failures_ >= policy_.max_failures) {
    locked_since_ms_ = now;
    logging::Warning("auth", "{} failed logins, locked for {} s", failures_,
                     policy_.lockout_ms / 1000);
  }
}

std::expected<SessionManager::Token, LoginError> SessionManager::Login(
    std::string_view username, std::string_view password) noexcept {
  const std::uint32_t now = platform_.now_ms();
  if (LockedOut(now)) {
    return std::unexpected(LoginError::kLockedOut);
  }
  // The key is derived even for a wrong user name, so the answer takes as
  // long.
  const bool password_matches = CheckPassword(password);
  const bool user_matches =
      ConstantTimeEqual(AsBytes(username), AsBytes(credentials_.username));
  if (!password_matches || !user_matches) {
    CountFailure(now);
    return std::unexpected(LoginError::kInvalidCredentials);
  }
  failures_ = 0;

  std::array<std::uint8_t, kTokenBytes> random{};
  if (platform_.random == nullptr || !platform_.random(random)) {
    return std::unexpected(LoginError::kNoRandom);
  }
  Session& session = FreeSlot(now);
  constexpr std::string_view kHex = "0123456789abcdef";
  for (std::size_t i = 0; i < random.size(); ++i) {
    session.token[2 * i] = kHex[random[i] >> 4];
    session.token[2 * i + 1] = kHex[random[i] & 0x0F];
  }
  session.last_used_ms = now;
  session.active = true;
  return session.token;
}

std::expected<void, PasswordError> SessionManager::ChangePassword(
    std::string_view current, std::string_view new_password,
    std::string_view keep_token) noexcept {
  const std::uint32_t now = platform_.now_ms();
  if (LockedOut(now)) {
    return std::unexpected(PasswordError::kLockedOut);
  }
  if (new_password.size() < kMinPasswordLength ||
      new_password.size() > kMaxPasswordLength) {
    return std::unexpected(PasswordError::kInvalidPassword);
  }
  if (!CheckPassword(current)) {
    CountFailure(now);
    return std::unexpected(PasswordError::kWrongPassword);
  }
  failures_ = 0;

  std::array<std::uint8_t, kNewSaltSize> salt{};
  if (platform_.random == nullptr || !platform_.random(salt)) {
    return std::unexpected(PasswordError::kNoRandom);
  }
  std::array<std::uint8_t, kMaxKeySize> key{};
  const std::span<std::uint8_t> derived =
      std::span(key).first(credentials_.derived_key.size());
  Pbkdf2HmacSha256(AsBytes(new_password), salt, credentials_.iterations,
                   derived);
  SetPasswordKey(salt, credentials_.iterations, derived);

  const Session* keep = Find(keep_token);
  for (Session& session : sessions_) {
    if (&session != keep) {
      session.active = false;
    }
  }
  return {};
}

bool SessionManager::SetPasswordKey(
    std::span<const std::uint8_t> salt, std::uint32_t iterations,
    std::span<const std::uint8_t> derived_key) noexcept {
  if (salt.empty() || salt.size() > kMaxSaltSize ||
      derived_key.size() < kMinKeySize || derived_key.size() > kMaxKeySize ||
      iterations == 0 || iterations > kMaxIterations) {
    return false;
  }
  std::copy(salt.begin(), salt.end(), salt_.begin());
  std::copy(derived_key.begin(), derived_key.end(), key_.begin());
  credentials_.salt = std::span(salt_).first(salt.size());
  credentials_.iterations = iterations;
  credentials_.derived_key = std::span(key_).first(derived_key.size());
  password_changed_ = true;
  return true;
}

bool SessionManager::Validate(std::string_view token) noexcept {
  Session* session = Find(token);
  if (session == nullptr) {
    return false;
  }
  const std::uint32_t now = platform_.now_ms();
  if (Expired(*session, now)) {
    session->active = false;
    return false;
  }
  session->last_used_ms = now;
  return true;
}

void SessionManager::Logout(std::string_view token) noexcept {
  if (Session* session = Find(token)) {
    session->active = false;
  }
}

bool SessionManager::Expired(const Session& session,
                             std::uint32_t now) const noexcept {
  return now - session.last_used_ms >= policy_.idle_timeout_ms;
}

SessionManager::Session* SessionManager::Find(
    std::string_view token) noexcept {
  if (token.size() != kTokenLength) {
    return nullptr;
  }
  for (Session& session : sessions_) {
    if (session.active &&
        ConstantTimeEqual(AsBytes({session.token.data(), kTokenLength}),
                          AsBytes(token))) {
      return &session;
    }
  }
  return nullptr;
}

SessionManager::Session& SessionManager::FreeSlot(std::uint32_t now) noexcept {
  Session* oldest = &sessions_.front();
  for (Session& session : sessions_) {
    if (!session.active || Expired(session, now)) {
      return session;
    }
    if (now - session.last_used_ms > now - oldest->last_used_ms) {
      oldest = &session;
    }
  }
  return *oldest;
}

std::string_view BearerToken(std::string_view authorization) noexcept {
  constexpr std::string_view kScheme = "Bearer ";
  if (authorization.size() <= kScheme.size() ||
      !http::EqualsIgnoreCase(authorization.substr(0, kScheme.size()),
                              kScheme)) {
    return {};
  }
  return http::Trim(authorization.substr(kScheme.size()));
}

}  // namespace webserver::auth
