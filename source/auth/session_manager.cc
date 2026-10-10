#include "webserver/auth/session_manager.h"

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

// Longest derived key compared; the build generates 32 bytes.
constexpr std::size_t kMaxKeySize = 64;

std::span<const std::uint8_t> AsBytes(std::string_view text) noexcept {
  return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}

}  // namespace

bool SessionManager::CheckPassword(std::string_view username,
                                   std::string_view password) const noexcept {
  std::array<std::uint8_t, kMaxKeySize> key{};
  const std::size_t key_size =
      credentials_.derived_key.size() < key.size()
          ? credentials_.derived_key.size()
          : key.size();
  const std::span<std::uint8_t> derived = std::span(key).first(key_size);
  // Derived even for a wrong user name, so the answer takes as long.
  Pbkdf2HmacSha256(AsBytes(password), credentials_.salt,
                   credentials_.iterations, derived);
  const bool key_matches = ConstantTimeEqual(
      derived, credentials_.derived_key.first(key_size));
  const bool user_matches =
      ConstantTimeEqual(AsBytes(username), AsBytes(credentials_.username));
  return key_size != 0 && key_matches && user_matches;
}

std::expected<SessionManager::Token, LoginError> SessionManager::Login(
    std::string_view username, std::string_view password) noexcept {
  const std::uint32_t now = platform_.now_ms();
  if (failures_ >= policy_.max_failures) {
    if (now - locked_since_ms_ < policy_.lockout_ms) {
      return std::unexpected(LoginError::kLockedOut);
    }
    failures_ = 0;
  }
  if (!CheckPassword(username, password)) {
    if (++failures_ >= policy_.max_failures) {
      locked_since_ms_ = now;
      logging::Warning("auth", "{} failed logins, locked for {} s", failures_,
                       policy_.lockout_ms / 1000);
    }
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
