// A minimal AIFF / AIFF-C reader for the reference recordings: uncompressed big-endian PCM (8,
// 16, 24 or 32 bit) or little-endian 'sowt', mixed to mono floats.
#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace aiff {

struct Audio { std::vector<float> mono; double rate = 44100.0; int channels = 1; };

inline uint32_t be32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }
inline uint16_t be16(const uint8_t* p) { return static_cast<uint16_t>((p[0] << 8) | p[1]); }
// The 80-bit IEEE extended float AIFF stores its sample rate in.
inline double extended(const uint8_t* p) {
  const int exponent = ((p[0] & 0x7f) << 8) | p[1];
  uint64_t mantissa = 0;
  for (int i = 0; i < 8; ++i) mantissa = (mantissa << 8) | p[2 + i];
  if (exponent == 0 && mantissa == 0) return 0.0;
  const double v = std::ldexp(static_cast<double>(mantissa), exponent - 16383 - 63);
  return (p[0] & 0x80) ? -v : v;
}

inline bool read_file(const char* path, Audio& out, std::string* error = nullptr) {
  std::FILE* f = std::fopen(path, "rb");
  if (!f) { if (error) *error = "cannot open"; return false; }
  std::vector<uint8_t> b;
  uint8_t chunk[1 << 16];
  size_t n;
  while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0) b.insert(b.end(), chunk, chunk + n);
  std::fclose(f);
  if (b.size() < 12 || std::memcmp(b.data(), "FORM", 4) != 0 || (std::memcmp(b.data() + 8, "AIFF", 4) != 0 && std::memcmp(b.data() + 8, "AIFC", 4) != 0)) {
    if (error) { *error = "not AIFF"; }
    return false;
  }
  int channels = 0, bits = 0;
  uint32_t frames = 0;
  bool little = false;
  const uint8_t* sound = nullptr;
  size_t sound_size = 0;
  for (size_t at = 12; at + 8 <= b.size();) {
    const uint32_t size = be32(b.data() + at + 4);
    const uint8_t* body = b.data() + at + 8;
    if (at + 8 + size > b.size()) break;
    if (std::memcmp(b.data() + at, "COMM", 4) == 0 && size >= 18) {
      channels = be16(body); frames = be32(body + 2); bits = be16(body + 6); out.rate = extended(body + 8);
      if (size >= 22 && std::memcmp(body + 18, "sowt", 4) == 0) little = true;
    } else if (std::memcmp(b.data() + at, "SSND", 4) == 0 && size >= 8) {
      const uint32_t offset = be32(body);
      sound = body + 8 + offset; sound_size = size - 8 - offset;
    }
    at += 8 + size + (size & 1);
  }
  if (!sound || channels <= 0 || bits <= 0 || bits > 32) { if (error) *error = "missing COMM or SSND"; return false; }
  const int bytes = (bits + 7) / 8;
  const size_t available = sound_size / static_cast<size_t>(bytes * channels);
  const size_t count = std::min<size_t>(frames, available);
  out.channels = channels;
  out.mono.assign(count, 0.0f);
  const double scale = 1.0 / std::ldexp(1.0, bits - 1);
  for (size_t i = 0; i < count; ++i) {
    double sum = 0.0;
    for (int c = 0; c < channels; ++c) {
      const uint8_t* s = sound + (i * static_cast<size_t>(channels) + static_cast<size_t>(c)) * static_cast<size_t>(bytes);
      int32_t v = 0;
      for (int k = 0; k < bytes; ++k) v = (v << 8) | (little ? s[bytes - 1 - k] : s[k]);
      v <<= (32 - 8 * bytes); v >>= (32 - bits);  // sign-extend
      sum += v * scale;
    }
    out.mono[i] = static_cast<float>(sum / channels);
  }
  return true;
}

}  // namespace aiff
