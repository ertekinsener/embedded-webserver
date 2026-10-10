#include "webserver/auth/sha256.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

namespace webserver::auth {
namespace {

constexpr std::array<std::uint32_t, 64> kRoundConstants{
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

constexpr std::array<std::uint32_t, 8> kInitialState{
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
};

constexpr std::uint32_t LoadBigEndian(const std::uint8_t* bytes) noexcept {
  return (std::uint32_t{bytes[0]} << 24) | (std::uint32_t{bytes[1]} << 16) |
         (std::uint32_t{bytes[2]} << 8) | std::uint32_t{bytes[3]};
}

}  // namespace

void Sha256::Reset() noexcept {
  state_ = kInitialState;
  block_size_ = 0;
  total_size_ = 0;
}

void Sha256::Compress(const std::uint8_t* block) noexcept {
  std::array<std::uint32_t, 64> w;
  for (std::size_t i = 0; i < 16; ++i) {
    w[i] = LoadBigEndian(block + i * 4);
  }
  for (std::size_t i = 16; i < 64; ++i) {
    const std::uint32_t s0 = std::rotr(w[i - 15], 7) ^
                             std::rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
    const std::uint32_t s1 = std::rotr(w[i - 2], 17) ^
                             std::rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }

  auto [a, b, c, d, e, f, g, h] = state_;
  for (std::size_t i = 0; i < 64; ++i) {
    const std::uint32_t s1 =
        std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
    const std::uint32_t choice = (e & f) ^ (~e & g);
    const std::uint32_t t1 = h + s1 + choice + kRoundConstants[i] + w[i];
    const std::uint32_t s0 =
        std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
    const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
    const std::uint32_t t2 = s0 + majority;
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }
  state_[0] += a;
  state_[1] += b;
  state_[2] += c;
  state_[3] += d;
  state_[4] += e;
  state_[5] += f;
  state_[6] += g;
  state_[7] += h;
}

void Sha256::Update(std::span<const std::uint8_t> data) noexcept {
  total_size_ += data.size();
  while (!data.empty()) {
    if (block_size_ == 0 && data.size() >= kBlockSize) {
      Compress(data.data());
      data = data.subspan(kBlockSize);
      continue;
    }
    const std::size_t take = std::min(kBlockSize - block_size_, data.size());
    std::copy_n(data.begin(), take, block_.begin() + block_size_);
    block_size_ += take;
    data = data.subspan(take);
    if (block_size_ == kBlockSize) {
      Compress(block_.data());
      block_size_ = 0;
    }
  }
}

Sha256Digest Sha256::Finish() noexcept {
  const std::uint64_t bit_length = total_size_ * 8;
  block_[block_size_++] = 0x80;
  if (block_size_ > kBlockSize - 8) {
    std::fill(block_.begin() + block_size_, block_.end(), 0);
    Compress(block_.data());
    block_size_ = 0;
  }
  std::fill(block_.begin() + block_size_, block_.end() - 8, 0);
  for (std::size_t i = 0; i < 8; ++i) {
    block_[kBlockSize - 1 - i] = static_cast<std::uint8_t>(bit_length >> (8 * i));
  }
  Compress(block_.data());

  Sha256Digest digest;
  for (std::size_t i = 0; i < state_.size(); ++i) {
    digest[i * 4] = static_cast<std::uint8_t>(state_[i] >> 24);
    digest[i * 4 + 1] = static_cast<std::uint8_t>(state_[i] >> 16);
    digest[i * 4 + 2] = static_cast<std::uint8_t>(state_[i] >> 8);
    digest[i * 4 + 3] = static_cast<std::uint8_t>(state_[i]);
  }
  Reset();
  return digest;
}

Sha256Digest Sha256Of(std::span<const std::uint8_t> data) noexcept {
  Sha256 sha;
  sha.Update(data);
  return sha.Finish();
}

HmacSha256::HmacSha256(std::span<const std::uint8_t> key) noexcept {
  std::array<std::uint8_t, Sha256::kBlockSize> block{};
  if (key.size() > block.size()) {
    const Sha256Digest digest = Sha256Of(key);
    std::copy(digest.begin(), digest.end(), block.begin());
  } else {
    std::copy(key.begin(), key.end(), block.begin());
  }

  std::array<std::uint8_t, Sha256::kBlockSize> pad;
  for (std::size_t i = 0; i < pad.size(); ++i) {
    pad[i] = block[i] ^ 0x36;
  }
  inner_.Update(pad);
  for (std::size_t i = 0; i < pad.size(); ++i) {
    pad[i] = block[i] ^ 0x5c;
  }
  outer_.Update(pad);
}

Sha256Digest HmacSha256::Compute(
    std::span<const std::uint8_t> message) const noexcept {
  Sha256 inner = inner_;
  inner.Update(message);
  const Sha256Digest inner_digest = inner.Finish();
  Sha256 outer = outer_;
  outer.Update(inner_digest);
  return outer.Finish();
}

void Pbkdf2HmacSha256(std::span<const std::uint8_t> password,
                      std::span<const std::uint8_t> salt,
                      std::uint32_t iterations,
                      std::span<std::uint8_t> output) noexcept {
  const HmacSha256 hmac(password);
  std::uint32_t block_index = 1;
  while (!output.empty()) {
    // U1 = HMAC(P, S || INT(i)); salts longer than 60 bytes are not needed.
    std::array<std::uint8_t, 64> first{};
    const std::size_t salt_size = std::min(salt.size(), first.size() - 4);
    std::copy_n(salt.begin(), salt_size, first.begin());
    for (std::size_t i = 0; i < 4; ++i) {
      first[salt_size + i] =
          static_cast<std::uint8_t>(block_index >> (8 * (3 - i)));
    }
    Sha256Digest u = hmac.Compute(std::span(first).first(salt_size + 4));
    Sha256Digest t = u;
    for (std::uint32_t round = 1; round < iterations; ++round) {
      u = hmac.Compute(u);
      for (std::size_t i = 0; i < t.size(); ++i) {
        t[i] ^= u[i];
      }
    }
    const std::size_t take = std::min(output.size(), t.size());
    std::copy_n(t.begin(), take, output.begin());
    output = output.subspan(take);
    ++block_index;
  }
}

bool ConstantTimeEqual(std::span<const std::uint8_t> lhs,
                       std::span<const std::uint8_t> rhs) noexcept {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  std::uint8_t difference = 0;
  for (std::size_t i = 0; i < lhs.size(); ++i) {
    difference |= lhs[i] ^ rhs[i];
  }
  return difference == 0;
}

}  // namespace webserver::auth
