// Minimal RIFF/WAVE reading and writing: 8/16/24/32-bit PCM and 32-bit float, any channel
// count (mixed to mono on read). Enough for sample import, the harvest tools and the tests,
// without a library dependency.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace fuzzboi::wav {

struct Audio {
  std::vector<float> mono;
  double rate = 48000.0;
  int source_channels = 1;
};

inline uint32_t le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24); }
inline uint16_t le16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }

// Parses a WAVE file image. Returns false with `error` set on anything it does not understand.
inline bool parse(const std::vector<uint8_t>& bytes, Audio& out, std::string* error = nullptr) {
  const auto fail = [&](const char* why) { if (error) *error = why; return false; };
  if (bytes.size() < 12 || std::memcmp(bytes.data(), "RIFF", 4) || std::memcmp(bytes.data() + 8, "WAVE", 4)) return fail("not a RIFF/WAVE file");
  size_t at = 12;
  uint16_t format = 0, channels = 0, bits = 0;
  uint32_t rate = 0;
  const uint8_t* data = nullptr;
  size_t data_size = 0;
  while (at + 8 <= bytes.size()) {
    const uint32_t size = le32(bytes.data() + at + 4);
    const uint8_t* body = bytes.data() + at + 8;
    const size_t available = std::min<size_t>(size, bytes.size() - at - 8);
    if (!std::memcmp(bytes.data() + at, "fmt ", 4)) {
      if (available < 16) return fail("short fmt chunk");
      format = le16(body); channels = le16(body + 2); rate = le32(body + 4); bits = le16(body + 14);
      if (format == 0xfffe && available >= 26) format = le16(body + 24);  // WAVE_FORMAT_EXTENSIBLE
    } else if (!std::memcmp(bytes.data() + at, "data", 4)) {
      data = body; data_size = available;
    }
    at += 8 + size + (size & 1);
  }
  if (!data || !channels || !rate) return fail("missing fmt or data chunk");
  const bool is_float = format == 3;
  if (!(format == 1 || is_float)) return fail("unsupported WAVE format tag");
  if (!(bits == 8 || bits == 16 || bits == 24 || bits == 32)) return fail("unsupported bit depth");
  const size_t frame_bytes = static_cast<size_t>(channels) * bits / 8;
  const size_t frames = data_size / frame_bytes;
  out.mono.assign(frames, 0.0f);
  out.rate = rate;
  out.source_channels = channels;
  for (size_t f = 0; f < frames; ++f) {
    double sum = 0.0;
    for (uint16_t c = 0; c < channels; ++c) {
      const uint8_t* s = data + f * frame_bytes + c * bits / 8;
      double v;
      if (bits == 8) v = (static_cast<int>(s[0]) - 128) / 128.0;
      else if (bits == 16) v = static_cast<int16_t>(le16(s)) / 32768.0;
      else if (bits == 24) v = (static_cast<int32_t>((s[0] << 8) | (s[1] << 16) | (static_cast<uint32_t>(s[2]) << 24)) >> 8) / 8388608.0;
      else if (is_float) { float x; std::memcpy(&x, s, 4); v = x; }
      else v = static_cast<int32_t>(le32(s)) / 2147483648.0;
      sum += v;
    }
    out.mono[f] = static_cast<float>(sum / channels);
  }
  return true;
}

inline bool read_file(const char* path, Audio& out, std::string* error = nullptr) {
  std::FILE* f = std::fopen(path, "rb");
  if (!f) { if (error) *error = "cannot open file"; return false; }
  std::vector<uint8_t> bytes;
  uint8_t chunk[65536];
  size_t n;
  while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0) bytes.insert(bytes.end(), chunk, chunk + n);
  std::fclose(f);
  return parse(bytes, out, error);
}

// Writes 16-bit PCM; `channels` interleaved buffers of equal length.
inline bool write_file(const char* path, const std::vector<std::vector<float>>& channels, double rate) {
  if (channels.empty()) return false;
  const size_t frames = channels[0].size();
  const uint16_t nch = static_cast<uint16_t>(channels.size());
  std::vector<uint8_t> bytes;
  const auto put32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) bytes.push_back(static_cast<uint8_t>(v >> (8 * i))); };
  const auto put16 = [&](uint16_t v) { bytes.push_back(static_cast<uint8_t>(v)); bytes.push_back(static_cast<uint8_t>(v >> 8)); };
  const uint32_t data_size = static_cast<uint32_t>(frames * nch * 2);
  bytes.insert(bytes.end(), {'R', 'I', 'F', 'F'}); put32(36 + data_size); bytes.insert(bytes.end(), {'W', 'A', 'V', 'E'});
  bytes.insert(bytes.end(), {'f', 'm', 't', ' '}); put32(16); put16(1); put16(nch); put32(static_cast<uint32_t>(rate));
  put32(static_cast<uint32_t>(rate) * nch * 2); put16(static_cast<uint16_t>(nch * 2)); put16(16);
  bytes.insert(bytes.end(), {'d', 'a', 't', 'a'}); put32(data_size);
  for (size_t f = 0; f < frames; ++f)
    for (const auto& ch : channels) {
      const float x = f < ch.size() ? ch[f] : 0.0f;
      put16(static_cast<uint16_t>(static_cast<int16_t>(std::lround(std::max(-1.0f, std::min(1.0f, x)) * 32767.0f))));
    }
  std::FILE* out = std::fopen(path, "wb");
  if (!out) return false;
  const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), out) == bytes.size();
  std::fclose(out);
  return ok;
}

}  // namespace fuzzboi::wav
