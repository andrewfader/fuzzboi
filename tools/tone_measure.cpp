// Prints the guitar-tone features of tests/tone_features.hpp for a raw stereo float32 file
// (interleaved, little-endian), as `ffmpeg -i in -f f32le -ac 2 -ar 44100 out.raw` writes it.
// Used to check the C++ measures against the corpus analysis (tools/tone_gate_profiles.py check);
// a review tool, not a test.
//
//   fuzzboi-tone-measure FILE.raw [RATE]
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "tone_features.hpp"

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: fuzzboi-tone-measure FILE.raw [RATE]\n"); return 2; }
  const double rate = argc > 2 ? std::atof(argv[2]) : 44100.0;
  std::FILE* in = std::fopen(argv[1], "rb");
  if (!in) { std::perror(argv[1]); return 1; }
  std::vector<float> left, right;
  float frame[2];
  while (std::fread(frame, sizeof(float), 2, in) == 2) { left.push_back(frame[0]); right.push_back(frame[1]); }
  std::fclose(in);
  const tone::Features f = tone::measure(left, right, rate);
  std::printf("{\"centroid_hz\": %.2f, \"rolloff85_hz\": %.2f, \"hf_share_db\": %.3f, \"flatness_db\": %.3f, \"crest100_db\": %.3f, \"stereo_corr\": %.4f, \"bands\": [",
              f.centroid_hz, f.rolloff85_hz, f.hf_share_db, f.flatness_db, f.crest100_db, f.stereo_corr);
  for (int b = 0; b < tone::kBands; ++b) std::printf("%s%.3f", b ? ", " : "", f.band_db[static_cast<size_t>(b)]);
  std::printf("]}\n");
  return 0;
}
