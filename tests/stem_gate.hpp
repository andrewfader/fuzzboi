// The stem gate's cases and its verdict, shared by tone_gate_tests and the tuning tool
// (tools/stem_gate_try.cpp): presets written for the lo-fi and home-psych records of the six-stem
// song corpus, each measured as the stem it would be separated into and held inside that
// group's interquartile range on the measures the stem is judged by.
#pragma once

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "stem_features.hpp"
#include "stem_reference.hpp"
#include "tone_gate.hpp"

namespace stem_gate {

struct Case { const char* preset; const char* group; const char* stem; tone::Phrase phrase; };

const std::vector<Case>& cases() {
  using tone::Phrase;
  static const std::vector<Case> c{
      {"4-track crunch (phys)", "GBV_lofi", "guitar", Phrase::Power},
      {"Home-psych fuzz (phys)", "OTC", "guitar", Phrase::Power},
      {"Home-taped round bass (phys)", "OTC", "bass", Phrase::Bassline},
      {"Home-psych singing saw", "OTC", "other", Phrase::Air},
      {"Home-psych tape strings", "OTC", "other", Phrase::Pad},
      {"Home-psych tape choir", "OTC", "other", Phrase::Air},
      {"Home-psych tape collage", "OTC_collage", "other", Phrase::Pad},
  };
  return c;
}

const stem::Reference* reference(const char* group, const char* stem_name) {
  for (const auto& r : stem::kReferences) if (std::strcmp(r.group, group) == 0 && std::strcmp(r.stem, stem_name) == 0) return &r;
  return nullptr;
}

// What each stem is judged by: a guitar by its brightness, its width and its wobble; a bass by
// how low and round it is; the other stem (saws, tape keys, organ) by its brightness and its
// wobble; a tape collage by everything that tells noise-like material apart: brightness, flatness,
// crest, width, and the wobble of the pitched fragments in it.
struct Measure { const char* name; double stem::Features::*value; stem::Stat stem::Reference::*stat; };
inline std::vector<Measure> measures(const char* group, const char* stem_name) {
  using F = stem::Features; using R = stem::Reference;
  const Measure centroid{"centroid Hz", &F::centroid_hz, &R::centroid_hz}, rolloff{"roll-off Hz", &F::rolloff85_hz, &R::rolloff85_hz};
  const Measure corr{"L/R correlation", &F::corr, &R::corr}, wobble{"wobble cents", &F::wobble_cents, &R::wobble_cents};
  const Measure flatness{"flatness dB", &F::flatness_db, &R::flatness_db}, crest{"crest dB", &F::crest100_db, &R::crest100_db};
  if (std::strcmp(group, "OTC_collage") == 0) return {centroid, rolloff, flatness, crest, corr, wobble};
  if (std::strcmp(stem_name, "guitar") == 0) return {centroid, rolloff, corr, wobble};
  if (std::strcmp(stem_name, "bass") == 0) return {centroid, rolloff};
  return {centroid, rolloff, wobble};
}

inline std::string judge(const stem::Features& f, const stem::Reference& r) {
  std::string failures;
  for (const Measure& m : measures(r.group, r.stem)) {
    const double v = f.*m.value;
    const stem::Stat& s = r.*m.stat;
    if (v >= s.lo && v <= s.hi) continue;
    char line[160];
    std::snprintf(line, sizeof(line), "%s %.4g outside %.4g..%.4g (records' median %.4g); ", m.name, v, s.lo, s.hi, s.median);
    failures += line;
  }
  return failures;
}

// How far outside the quartiles, in interquartile ranges, with a light pull to the median: for tuning.
inline double loss(const stem::Features& f, const stem::Reference& r) {
  double sum = 0.0;
  for (const Measure& m : measures(r.group, r.stem)) {
    const double v = f.*m.value;
    const stem::Stat& s = r.*m.stat;
    const double iqr = std::max(s.hi - s.lo, 1e-6);
    sum += 3.0 * (std::max(0.0, s.lo - v) + std::max(0.0, v - s.hi)) / iqr + 0.15 * std::abs(v - s.median) / iqr;
  }
  return sum;
}

// The phrase as the gate plays it: sixteen seconds, the first half second left out, brought to
// -20 dBFS (none of the judged measures depends on the level).
inline tone::Stereo render_case(const fuzzboi::Settings& settings, tone::Phrase phrase) {
  auto audio = tone::play(settings, phrase, tone_gate::kSeconds, tone_gate::kRate);
  const size_t from = static_cast<size_t>(0.5 * tone_gate::kRate), to = static_cast<size_t>(tone_gate::kSeconds * tone_gate::kRate);
  tone::Stereo out{{audio.left.begin() + static_cast<long>(from), audio.left.begin() + static_cast<long>(to)},
                   {audio.right.begin() + static_cast<long>(from), audio.right.begin() + static_cast<long>(to)}};
  double sum = 0.0;
  for (size_t i = 0; i < out.left.size(); ++i) { const double m = 0.5 * (out.left[i] + out.right[i]); sum += m * m; }
  const double rms = std::sqrt(sum / static_cast<double>(std::max<size_t>(out.left.size(), 1)));
  const float gain = static_cast<float>(0.1 / std::max(rms, 1e-12));
  for (size_t i = 0; i < out.left.size(); ++i) { out.left[i] *= gain; out.right[i] *= gain; }
  return out;
}

}  // namespace stem_gate
