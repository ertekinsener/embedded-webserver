#ifndef WEBSERVER_INCLUDE_WEBSERVER_AUTH_SHA256_
#define WEBSERVER_INCLUDE_WEBSERVER_AUTH_SHA256_

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

// SHA-256 (FIPS 180-4), HMAC-SHA-256 (RFC 2104) and PBKDF2-HMAC-SHA-256
// (RFC 8018), enough to check a password against a stored derived key
// without a crypto library. Not constant time with respect to the data
// length, which is not secret here.
namespace webserver::auth {

using Sha256Digest = std::array<std::uint8_t, 32>;

class Sha256 {
 public:
  static constexpr std::size_t kBlockSize = 64;

  Sha256() noexcept { Reset(); }

  void Reset() noexcept;
  void Update(std::span<const std::uint8_t> data) noexcept;
  Sha256Digest Finish() noexcept;

 private:
  void Compress(const std::uint8_t* block) noexcept;

  std::array<std::uint32_t, 8> state_{};
  std::array<std::uint8_t, kBlockSize> block_{};
  std::size_t block_size_ = 0;
  std::uint64_t total_size_ = 0;
};

Sha256Digest Sha256Of(std::span<const std::uint8_t> data) noexcept;

// HMAC with the key pads hashed once, so each Compute() costs two
// compressions for a short message (PBKDF2 calls it per iteration).
class HmacSha256 {
 public:
  explicit HmacSha256(std::span<const std::uint8_t> key) noexcept;

  Sha256Digest Compute(std::span<const std::uint8_t> message) const noexcept;

 private:
  Sha256 inner_;
  Sha256 outer_;
};

// Derives `output.size()` bytes.
void Pbkdf2HmacSha256(std::span<const std::uint8_t> password,
                      std::span<const std::uint8_t> salt,
                      std::uint32_t iterations,
                      std::span<std::uint8_t> output) noexcept;

// Compares in time independent of where the inputs differ.
bool ConstantTimeEqual(std::span<const std::uint8_t> lhs,
                       std::span<const std::uint8_t> rhs) noexcept;

}  // namespace webserver::auth

#endif  // WEBSERVER_INCLUDE_WEBSERVER_AUTH_SHA256_
