#pragma once

// Public-domain SHA-1 (RFC 3174).
// Vendored so iwars_sim does not need system OpenSSL.
// Used only for the RFC6455 WebSocket Sec-WebSocket-Accept key.

#include <cstdint>
#include <cstring>

namespace sha1_detail {

inline std::uint32_t rol(std::uint32_t value, int bits) {
  return (value << bits) | (value >> (32 - bits));
}

inline void transform(std::uint32_t state[5], const std::uint8_t buffer[64]) {
  std::uint32_t w[80];
  for (int i = 0; i < 16; ++i) {
    w[i] = (static_cast<std::uint32_t>(buffer[i * 4]) << 24) |
           (static_cast<std::uint32_t>(buffer[i * 4 + 1]) << 16) |
           (static_cast<std::uint32_t>(buffer[i * 4 + 2]) << 8) |
           static_cast<std::uint32_t>(buffer[i * 4 + 3]);
  }
  for (int i = 16; i < 80; ++i) {
    w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
  }

  std::uint32_t a = state[0];
  std::uint32_t b = state[1];
  std::uint32_t c = state[2];
  std::uint32_t d = state[3];
  std::uint32_t e = state[4];

  for (int i = 0; i < 80; ++i) {
    std::uint32_t f = 0;
    std::uint32_t k = 0;
    if (i < 20) {
      f = (b & c) | ((~b) & d);
      k = 0x5A827999u;
    } else if (i < 40) {
      f = b ^ c ^ d;
      k = 0x6ED9EBA1u;
    } else if (i < 60) {
      f = (b & c) | (b & d) | (c & d);
      k = 0x8F1BBCDCu;
    } else {
      f = b ^ c ^ d;
      k = 0xCA62C1D6u;
    }
    const std::uint32_t temp = rol(a, 5) + f + e + k + w[i];
    e = d;
    d = c;
    c = rol(b, 30);
    b = a;
    a = temp;
  }

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
}

}  // namespace sha1_detail

inline constexpr int SHA1_DIGEST_LENGTH = 20;

inline void sha1(const void* data, std::size_t len, std::uint8_t out[SHA1_DIGEST_LENGTH]) {
  std::uint32_t state[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u,
                            0xC3D2E1F0u};
  const auto* bytes = static_cast<const std::uint8_t*>(data);
  const std::uint64_t bit_len = static_cast<std::uint64_t>(len) * 8;

  while (len >= 64) {
    sha1_detail::transform(state, bytes);
    bytes += 64;
    len -= 64;
  }

  std::uint8_t block[64];
  std::memcpy(block, bytes, len);
  block[len] = 0x80;
  if (len + 1 <= 56) {
    std::memset(block + len + 1, 0, 56 - (len + 1));
  } else {
    std::memset(block + len + 1, 0, 64 - (len + 1));
    sha1_detail::transform(state, block);
    std::memset(block, 0, 56);
  }
  for (int i = 0; i < 8; ++i) {
    block[63 - i] = static_cast<std::uint8_t>(bit_len >> (8 * i));
  }
  sha1_detail::transform(state, block);

  for (int i = 0; i < 5; ++i) {
    out[i * 4] = static_cast<std::uint8_t>(state[i] >> 24);
    out[i * 4 + 1] = static_cast<std::uint8_t>(state[i] >> 16);
    out[i * 4 + 2] = static_cast<std::uint8_t>(state[i] >> 8);
    out[i * 4 + 3] = static_cast<std::uint8_t>(state[i]);
  }
}
